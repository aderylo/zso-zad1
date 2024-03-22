#!/usr/bin/env python3
from __future__ import annotations

import argparse
import dataclasses
import logging
import os
import re
import shlex
import shutil
import sys
import traceback
import typing
import warnings
from enum import Enum
from functools import wraps
from operator import attrgetter
from pathlib import Path
from pprint import pformat
from runpy import run_path
from subprocess import check_call, CompletedProcess, run, DEVNULL
from tempfile import TemporaryDirectory
from typing import Optional, Union, Literal, NamedTuple, Iterable, Any, TypeVar, Callable, Iterator, Sequence, Tuple

logger = logging.getLogger(__name__)

try:
    from elftools.elf.constants import SH_FLAGS
    from elftools.elf.elffile import ELFFile
    from elftools.elf.enums import ENUM_RELOC_TYPE_i386
    from elftools.elf.relocation import Relocation
    from elftools.elf.sections import Section, Symbol
    from elftools.elf.segments import Segment
except ImportError:
    raise ImportError("This script requires the pyelftools package to be installed.\n"
                      "Alternatively, try using the .pex self-contained version of this script.")

ROOT_PATH = Path(os.environ.get('PEX', __file__)).parent
HACKY_RELPATH: Optional[Path] = None
def GitPath(path='') -> Path:
    """Hacky path constructor that reinterprets absolute paths, which are relative to the git repo.

    When relative paths are created, their base may be overridden with global HACKY_RELPATH.
    """
    if isinstance(path, Path):
        return path

    path = Path(path)
    if path.is_absolute():
        path = ROOT_PATH / path.relative_to(path.anchor)
    elif HACKY_RELPATH:
        path = HACKY_RELPATH / path
    return path

GCC_FOR_LD = 'x86_64-linux-gnu-gcc-10'
LD_BASE_FLAGS = ['-march=lakemont', '-mtune=lakemont', '-m32', '-miamcu', '-msoft-float',
                 '-nostdlib',  '-Wl,--no-relax', '-Wl,--emit-relocs',  '-Wl,--build-id=none',
                 # '-Wl,--no-warn-rwx-segments',
                 '-Wl,--strip-debug',
                 '-T', GitPath('/picolibc/iamcu/lib/picolibc.ld')
                 ]
LD_STATIC_FLAGS = ['-Wl,-znorelro,-ztext', '-static']

# GNU objcopy seems to improperly handle ET_EXEC with static relocation sections
OBJCOPY = 'llvm-objcopy'
OBJCOPY_FLAGS = ['-O', 'binary', '--gap-fill', '0x90']

if not typing.TYPE_CHECKING:
    if not shutil.which(GCC_FOR_LD):
        raise ImportError(f"Cannot find {GCC_FOR_LD} in PATH")
    if not shutil.which(OBJCOPY):
        if shutil.which(OBJCOPY + '-16'):
            OBJCOPY = OBJCOPY + '-16'
        else:
            raise ImportError(f"Cannot find {OBJCOPY} in PATH. The llvm version is required.")

TRACE_COMMANDS = True
if TRACE_COMMANDS:
    @wraps(run)
    def run(cmd, *args, **kwargs):
        print(shlex.join(map(str, cmd)), file=sys.stderr)
        return run.__wrapped__(cmd, *args, **kwargs)

    @wraps(check_call)
    def check_call(cmd, *args, **kwargs):
        print(shlex.join(map(str, cmd)), file=sys.stderr)
        return check_call.__wrapped__(cmd, *args, **kwargs)

# Make a modern Enum
dict_style_enum = ENUM_RELOC_TYPE_i386.copy()
del dict_style_enum['_default_']

RELOC_TYPE_I386 = Enum('RELOC_TYPE_I386', dict_style_enum)

def link_relocatable(partial_path: Path, dst: Path, mode: Literal['static'] = 'static',
                     extra_args: Union[list[str], dict[str, str]]=()) -> Sequence[str]:
    """Link and return extra flags on success"""
    cmd = [GCC_FOR_LD]
    cmd.extend(LD_BASE_FLAGS)
    if mode == 'static':
        cmd.extend(LD_STATIC_FLAGS)
    else:
        raise ValueError(f"Unknown mode {mode}")
    if isinstance(extra_args, (list,tuple)):
        extra_flags = extra_args
    elif isinstance(extra_args, dict):
        extra_flags = [f'-Wl,{k}={v}' for k, v in extra_args.items()]
    elif extra_args is None:
        extra_flags = ()
    else:
        raise ValueError(f"Cannot process extra flags {extra_args}")

    cmd.extend([partial_path, '-o', dst])
    check_call(cmd)
    return extra_flags


def built_keep_function_symbols(elf_path: Path):
    # We leave this file for reference
    tmp_file = elf_path.with_suffix(elf_path.suffix + '.symbols-to-strip')
    with tmp_file.open('w') as f:
        for symtab in ELFFile.load_from_path(elf_path).iter_sections('SHT_SYMTAB'):
            for sym in symtab.iter_symbols():
                if sym['st_info']['type'] != 'STT_FUNC':
                    print(sym.name, file=f)
    return ['--strip-unneeded-symbols', tmp_file]

STRIP_LEVEL_FLAGS=dict(
    binary=['--strip-non-alloc', '-O', 'binary',],
    strip=['--strip-sections'],
    sections=['--strip-all'],
    function_symbols=built_keep_function_symbols,
    symbols=['-R', '.rel.*', '-R', '!.rel.dyn'],
    add_relocations=[],
)

def strip_elf_exec_to_level(elf_path: Path, dst: Path, level: str):
    cmd = [OBJCOPY]
    level_flags = STRIP_LEVEL_FLAGS[level]
    if callable(level_flags):
        # This needs two levels!
        elf_path = strip_elf_exec_to_level(elf_path, dst, 'symbols')
        level_flags = level_flags(elf_path)
    cmd.extend(level_flags)
    check_call(cmd + [elf_path, dst])
    return dst

class MainSegments(NamedTuple):
    text: Segment
    data: Segment
    bss: Segment

class ExecReloc(NamedTuple):
    vaddr: int
    value: int
    type_: RELOC_TYPE_I386
    symbol: Symbol

    def __str__(self):
        return f"ExecReloc({self.type_.name} @ 0x{self.vaddr:08x} => 0x{self.value:08x} '{self.symbol.name}')"

class RelReloc(NamedTuple):
    vaddr: int
    value: int | str
    type_: RELOC_TYPE_I386
    symbol: Symbol

    def __str__(self):
        target = f"0x{self.value:08x}" if isinstance(self.value, int) else self.value
        return f"RelReloc({self.type_.name} @ 0x{self.vaddr:08x} => {target} '{self.symbol.name}')"

@dataclasses.dataclass
class BinFile:
    elf: Path
    part: Optional[Path] = None
    _stripped: Path = None
    _parsed: dict[str, ELFFile] = dataclasses.field(default_factory=dict)
    link_extra_flags: Sequence[str] = dataclasses.field(default=())

    @property
    def strip(self):
        if self._stripped is None:
            self._stripped = strip_elf_exec_to_level(self.elf, self.elf.with_suffix(self.elf.suffix + '.strip'), 'strip')
        return self._stripped

    def __post_init__(self):
        self.validate()

    def validate(self):
        assert self.elf.exists()
        if self.part:
            assert self.part.exists()
            assert self.part.stat().st_mtime < self.elf.stat().st_mtime
        if self._stripped:
            assert self._stripped.exists()
            assert self.elf.stat().st_mtime < self._stripped.stat().st_mtime

    @classmethod
    def from_test(cls, elf_base: Path) -> BinFile:
        res = cls(elf=elf_base)
        partial_link = elf_base.with_suffix('.part')
        if partial_link.exists():
            res.part = partial_link
        res.validate()
        return res

    @classmethod
    def from_relocatable(cls, partial_elf: Path, link_extra_flags: Union[list, dict] = ()) -> BinFile:
        elf_path = partial_elf.with_suffix(partial_elf.suffix + '.elf')
        flags = link_relocatable(partial_elf, elf_path, mode='static', extra_args=link_extra_flags)
        res = cls(elf=elf_path, part=partial_elf, link_extra_flags=flags)
        return res

    def kind(self, on: Literal['elf', 'part', 'strip']):
        # match is from 3.5...
        res = getattr(self, on)
        if not res:
            raise KeyError(f"Kind {on} is not present in {self}")
        return res

    def parsed(self, on: Literal['elf', 'part', 'strip']):
        if on not in self._parsed:
            # This leaks fd, but whatever
            self._parsed[on] = ELFFile.load_from_path(self.kind(on))
        return self._parsed[on]

    def get_main_segments(self) -> MainSegments:
        text = None
        data = None
        bss = None
        for segment in self.parsed('strip').iter_segments(type='PT_LOAD'):
            segment: Segment
            if segment['p_filesz'] == 0:
                bss = segment
            elif segment['p_vaddr'] == 0x40030000:
                text = segment
            elif segment['p_vaddr'] == 0xa800a000 and segment['p_vaddr'] != segment['p_paddr']:
                data = segment
            else:
                warnings.warn(f"Ignoring segment {segment} at offset {segment['p_offset']:#x}")

        return MainSegments(text, data, bss)

    def iter_relocations(self, on: Literal['elf', 'part'] = 'elf') -> Iterable[Any]:
        elf = self.parsed(on)
        for sh_rel in elf.iter_sections('SHT_REL'):
            section = elf.get_section(sh_rel['sh_info'])
            symtab = elf.get_section(sh_rel['sh_link'])
            for rel in sh_rel.iter_relocations():
                sym_idx = rel['r_info_sym']
                if sym_idx > symtab.num_symbols():
                    raise RuntimeError(f"Relocation {rel} for {section.name} is malformed.")
                symbol = symtab.get_symbol(sym_idx)
                type_ = RELOC_TYPE_I386(rel['r_info_type'])

                if on == 'elf':
                    assert elf['e_type'] == 'ET_EXEC'
                    reloc_spec = ExecReloc(rel['r_offset'], symbol['st_value'], type_, symbol)
                    yield reloc_spec
                else:
                    assert elf['e_type'] == 'ET_REL'
                    # There is no strict notion of virtual addresses in relocatable files
                    section_vaddr = self.resolve_section_vaddr(rel, section)
                    rel_vaddr = section_vaddr + rel['r_offset']

                    sym_section_idx = symbol['st_shndx']
                    if isinstance(sym_section_idx, str):
                        target = symbol['st_value'] if sym_section_idx == 'SHN_ABS' else sym_section_idx
                    else:
                        target_section_vaddr = self.resolve_section_vaddr(symbol, elf.get_section(sym_section_idx))
                        target = symbol['st_value'] + target_section_vaddr

                    reloc_spec = RelReloc(rel_vaddr, target, type_, symbol)
                    yield reloc_spec

    @staticmethod
    def resolve_section_vaddr(why: Relocation | Symbol | int, section):
        if section['sh_addr']:
            # assume this was the correct virtual addr
            section_vaddr = section['sh_addr']
        elif match := re.match(r'\.[\w\d.]+\.([\da-fA-F]{8})\w?', section.name):
            # e.g., from .text.40030000f
            section_vaddr = int(match.group(1), 16)
        else:
            raise ValueError(f"Cannot resolve virtual address for {why} of {section.name}")
        return section_vaddr

    def find_section_for_vaddr(self, on: Literal['part', 'elf'], vaddr: int) -> Section:
        for section in self.parsed(on).iter_sections():
            if not section['sh_flags'] & SH_FLAGS.SHF_ALLOC:
                continue
            try:
                found_addr = self.resolve_section_vaddr(vaddr, section)
                if found_addr == vaddr:
                    return section
            except ValueError:
                pass
        else:
            raise IndexError(f"No section matching exactly {vaddr=:#x} found in {self}")


@dataclasses.dataclass
class Comparator:
    truth: BinFile
    symbolized: BinFile

    def show_diff(self, on: Literal['elf', 'part', 'strip'], cmd: list[str]):
        # check_call(['../run_ccdiff.sh', self.symbolized.kind(on), self.truth.kind(on),] + cmd)
        with TemporaryDirectory() as tmpdir:
            tmpdir = Path(tmpdir)
            truth = tmpdir / 'truth'
            with truth.open('wb') as f:
                check_call(cmd + [self.truth.kind(on)], stdout=f)
            symbolized = tmpdir / 'symbolized'
            with symbolized.open('wb') as f:
                check_call(cmd + [self.symbolized.kind(on)], stdout=f)
            run(['diff', '-u', symbolized, truth])

    @staticmethod
    def _compare_segments(true: Segment, pred: Segment) -> list[str]:
        if pred is None and true is not None:
            return [f'Symbolized file has no segment at {true["p_vaddr"]}']
        elif pred is not None and true is None:
            return [f'Symbolized file should not have a segment at {pred["p_vaddr"]}']
        elif pred is None and true is None:
            return []

        errs = []
        CHECKED_FIELDS = ('p_vaddr', 'p_paddr', 'p_filesz', 'p_memsz')
        for field in CHECKED_FIELDS:
            if true[field] != pred[field]:
                errs.append(f'Mismatch on {field}: expected {true[field]:#x} got {pred[field]:#x} (diff={true[field]-pred[field]:#x})')
        if (td := true.data()) != (pd := pred.data()):
            if len(td) != len(pd):
                errs.append('Content length mismatch')
            else:
                # numpy please
                # TODO: allow .got permutation!
                mismatched_bytes = sum(t != p for t, p in zip(td, pd))
                errs.append(f'Content mismatch by {mismatched_bytes} ({mismatched_bytes/len(td):.3%})')
                if mismatched_bytes / len(td) < 0.05:
                    errs.append(f'Note: .got permutation is allowed, but this is not yet whitelisted!')
        return errs

    def compare_segments(self, verbose=False) -> bool:
        truth = self.truth.get_main_segments()
        symbolized = self.symbolized.get_main_segments()
        fail = False
        logger.debug(f"Comparing {truth=} vs {symbolized=}")

        for field, true, pred in zip(MainSegments._fields, truth, symbolized):
            res = self._compare_segments(true, pred)
            fail |= bool(res)
            if verbose and (pred is not None or true is not None):
                res = res or 'OK'
                print(f"For {field} at offset: {(pred or true)['p_offset']:#x}", pformat(res))

        return fail

    def compare_relocations(self, pred_on='part', verbose=False) -> float:
        key = attrgetter('vaddr')
        true = sorted(self.truth.iter_relocations('elf'), key=key)
        pred = sorted(self.symbolized.iter_relocations(pred_on), key=key)
        assert len(true) == len(set(map(key, true))), "Ground truth has duplicate relocation for an address!"
        assert len(pred) == len(set(map(key, pred))), "Symbolized has duplicate relocation for an address!"

        # Procedural expansion should be easier to comprehend
        match, false_positive, false_negative, mismatch = 0, 0, 0, 0
        for true_rel, pred_rel in merge_sorted(true, pred, key):
            if true_rel is None:
                false_positive += 1
                if verbose:
                    print(f"Superfluous relocation {pred_rel}")
                continue
            elif true_rel.type_ == RELOC_TYPE_I386.R_386_RELATIVE:
                # skip
                continue
            if pred_rel is None:
                false_negative += 1
                if verbose:
                    print(f"Missing relocation {true_rel}")
                continue

            assert true_rel.vaddr == pred_rel.vaddr
            value_match = true_rel.value == pred_rel.value
            type_match = true_rel.type_ == pred_rel.type_

            if value_match and type_match:
                if pred_rel.symbol['st_shndx'] == 'SHN_ABS' and true_rel.value:
                    if verbose:
                        extra = f"(originally {true_name}) " if (true_name := true_rel.symbol.name) else ""
                        print(f"Relocation against an ABS symbol {extra}is probably not what you want: {pred_rel}.")
                    mismatch += 1
                else:
                    # A simple case
                    match += 1
            elif pred_rel.value == 'SHN_UNDEF' and type_match and true_rel.type_ == RELOC_TYPE_I386.R_386_GOTPC:
                # gotpc should be undef
                match += 1
            elif value_match and true_rel.type_ == RELOC_TYPE_I386.R_386_PLT32 and pred_rel.type_ == RELOC_TYPE_I386.R_386_PC32:
                # TODO: need a check if the symbol was actually collapsed
                match += 1
            elif type_match and true_rel.symbol['st_info']['type'] == 'STT_SECTION':
                if verbose:
                    print(f"Note: Object relocations are not checking the destination address currently: {pred_rel}.")
                match += 1
            else:
                if verbose:
                    print(f"Mismatched rels at {true_rel.vaddr:#x}: expected {true_rel} (or compatible) got {pred_rel}")
                mismatch += 1

        # Actually, mismatch should be multiplied by 2
        # But let's make the score with no false positives equal to recall
        iou = match / (match + false_positive + false_negative + mismatch)
        return iou

# This should be Pydantic, but that's another non-pure dependency
@dataclasses.dataclass
class SectionReplacement:
    # pass one
    bin_file: Optional[str] = None
    from_elf_section: Optional[Tuple[str, str]] = None

    # pass one
    section_name: Optional[str] = None
    symbol_name: Optional[str] = None
    vaddr: Optional[int] = None

    def get_or_create_bin_file(self) -> Path:
        if self.bin_file:
            return GitPath(self.bin_file)
        elf_path, section_name = self.from_elf_section
        elf_path = GitPath(elf_path)
        # We keep this file for reference
        new_bin_file = elf_path.with_suffix(f'{elf_path.suffix}.{section_name}.bin')
        new_bin_file.write_bytes(ELFFile.load_from_path(elf_path).get_section_by_name(section_name).data())
        return new_bin_file

    def get_replaced_section_name(self, c: Comparator):
        if self.section_name:
            return self.section_name

        if self.symbol_name:
            parsed = c.truth.parsed('elf')
            for symbtab in parsed.iter_sections('SHT_SYMTAB'):
                if sym := symbtab.get_symbol_by_name(self.symbol_name):
                    # Not a special index
                    if sym[0]['st_shndx'] != 'SHN_ABS':
                        vaddr = sym[0]['st_value']
                        break
            else:
                raise KeyError(f"Symbol {self.symbol_name} not found in {c.truth.elf}")
        else:
            vaddr = self.vaddr

        return c.symbolized.find_section_for_vaddr('part', vaddr).name

    def modify_inplace(self, elf_path: Path, c: Comparator):
        data_file = self.get_or_create_bin_file()
        section_name = self.get_replaced_section_name(c)
        replace_section(elf_path, elf_path, section_name, data_file)
        rel_name = f'.rel{section_name}'
        if c.symbolized.parsed('part').get_section_by_name(rel_name):
            remove_section(elf_path, elf_path, rel_name)

def replace_section(src: Path, dst: Path, section_name: str, data_file: Path):
    check_call([OBJCOPY, '--update-section', f'{section_name}={data_file}', src, dst])

def remove_section(src: Path, dst: Path, section_name: str):
    check_call([OBJCOPY, '--remove-section', section_name, src, dst])

@dataclasses.dataclass
class OverrideFromObject:
    """Normally when linking multiple files with same named sections, they are concatenated.

    The section names and symbols will be renamed to match the resulting one.
    Grouping, on the other hand, requires special handling.
    """
    obj_file: str

    def prepare(self, c: Comparator) -> Path:
        raise NotImplementedError("Not yet implemented fully")

def execute(bin: Path, stdin: Optional[Path]) -> CompletedProcess:
    # TODO: run in qemu
    kwargs = {}
    # To make sure the program is not modifying it
    if stdin is not None:
        kwargs['input'] = stdin.read_bytes()
    else:
        kwargs['stdin'] = DEVNULL

    return run(['i386', bin if bin.is_absolute() else f"./{bin}"], capture_output=True, timeout=10, )


@dataclasses.dataclass
class RunSpec:
    exit_code: Optional[int]
    #: Path to file
    expected_stdout: Optional[str] = None
    stdin: Optional[str] = None

    def compare_output(self, bin: BinFile) -> list[str]:
        errs = []
        result = execute(bin.elf, self.stdin and Path(self.stdin))
        if self.exit_code is not None and result.returncode != self.exit_code:
            errs.append(f"Return code mismatch: {result.returncode} instead of {self.exit_code}")

        if self.expected_stdout:
            data = GitPath(self.expected_stdout).read_bytes()
            if data != result.stdout:
                if data.startswith(result.stdout):
                    errs.append("Stdout mismatch, but the result is a prefix.")
                elif result.stdout.startswith(data):
                    errs.append("Stdout mismatch, but the expected one is a prefix.")
                else:
                    errs.append("Stdout mismatch.")
        return errs

@dataclasses.dataclass
class ReplacementSpec:
    behavior: RunSpec
    replacements: Sequence[SectionReplacement] = ()
    obj_override: Optional[OverrideFromObject] = None

    def compare_output(self, bin: BinFile) -> list[str]:
        return self.behavior.compare_output(bin)

    def replace_and_link(self, c: Comparator) -> BinFile:
        new_rel = c.symbolized.elf.with_suffix('.hax.part')
        link_args = c.symbolized.link_extra_flags
        shutil.copyfile(c.symbolized.part, new_rel)
        for section_replacement in self.replacements:
            section_replacement.modify_inplace(new_rel, c)
        if self.obj_override:
            link_args = link_args + self.obj_override.prepare(new_rel, c)
        return BinFile.from_relocatable(new_rel, link_args)

@dataclasses.dataclass
class TestSpec:
    link_mode: Literal['static', 'dynamic']
    links_picolibc: bool
    pic: Literal['unrestricted', 'no-plt', 'simple', 'none']
    extra_cflags: Sequence[str]
    unmodified_behavior: RunSpec
    replacement: ReplacementSpec

    # All the functions return truthy value on error.
    def evaluate_relinking(self, c: Comparator, verbose=True) -> bool:
        rebuild_fail = c.compare_segments(verbose=verbose)
        execution_errs = self.unmodified_behavior.compare_output(c.symbolized)
        if execution_errs and verbose:
            print('Relinking execution errors:')
            print('\n'.join(execution_errs))

        return bool(rebuild_fail) or bool(execution_errs)

    def evaluate_replacement(self, c: Comparator, verbose=True) -> bool:
        # all flags ar in c.symbolized
        try:
            new_bin: BinFile = self.replacement.replace_and_link(c)
        except:
            traceback.print_exc()
            return True

        errs = self.replacement.compare_output(new_bin)
        if verbose and errs:
            print('Replacement errors:')
            print('\n'.join(errs))
        return bool(errs)

    def merge_flags(self, extra_ld_flags):
        assert not extra_ld_flags
        # TODO: implement this
        if self.link_mode == 'static':
            return ('static',)
        return ()

    def test_validation_sanity_check(self, test_bin: BinFile, verbose=True) -> bool:
        errs = self.unmodified_behavior.compare_output(test_bin)
        if errs and verbose:
            print('TestCase smoke test errors:')
            print('\n'.join(errs))
        return bool(errs)


    def score_symbolization(self, test_bin: BinFile, symbolized: Path, extra_ld_flags=(), verbose=True) \
            -> Tuple[Comparator, float]:
        flags = self.merge_flags(extra_ld_flags)
        bin = BinFile.from_relocatable(symbolized, flags)
        cmp = Comparator(test_bin, bin)

        if verbose:
            print('\n-- RELOCATIONS BASE SCORE --')
        try:
            base_score = cmp.compare_relocations(verbose=verbose)
        except ValueError:
            traceback.print_exc()
            print("\nAttempting to continue...")
            base_score = 0.0

        print(f"Relocations score:", "ALL" if base_score == 1.0 else f"{base_score:.3f}")

        if verbose:
            print("\n -- RELINKING --\n"
                  "Segment equivalence issues (this should be enough to prove no behavior change):")

        # XXX: this doesn't properly check GOT permutations yet
        rebuild_fail = self.evaluate_relinking(cmp, verbose=verbose)
        if rebuild_fail:
            base_score /= 3
        print(f"Relinking (step 2):", "fail? (read error comments)" if rebuild_fail else "OK")

        if verbose:
            print("\n -- REPLACEMENT --")
        replacement_fail = self.evaluate_replacement(cmp, verbose)
        if replacement_fail:
            base_score /= 2

        print(f"Replacement (step 3):", "FAIL" if replacement_fail else "OK")

        return cmp, base_score

TV = TypeVar('TV')
def merge_sorted(it1: Iterable[TV], it2: Iterable[TV], key: Callable[[TV,], Any] = lambda x:x) -> Iterator[TV]:
    """A helper function like ``zip``, but works on sorted (according to ``key``) iterators
    and yields a pair only if the key match. Otherwise, it yields None on one side.

    ``key(value)`` should never return ``None``.
    """
    it1, it2 = iter(it1), iter(it2)
    kl = None
    kr = None
    while True:
        if kr is None:
            try:
                r = next(it2)
                kr = key(r)
            except StopIteration:
                if kl is not None:
                    yield l, None
                yield from ((l, None) for l in it1)
                return

        if kl is None:
            try:
                l = next(it1)
                kl = key(l)
            except StopIteration:
                yield None, r
                yield from  ((None, r) for r in it2)
                return

        if kl == kr:
            yield l, r
            kl, kr = None, None
        elif kl < kr:
            yield l, None
            kl = None
        else:
            yield None, r
            kr = None



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Check the solution.")
    parser.add_argument('symbolized_rel', type=Path)
    parser.add_argument('ground_truth_exec', type=Path)
    parser.add_argument('--quiet', '-q', action='store_false', dest='verbose')
    args = parser.parse_args()

    if args.verbose is False and hasattr(run, '__wrapped__'):
        # TODO: make it in a proper way
        run = run.__wrapped__
        check_call = check_call.__wrapped__

    # TODO: load this from file
    if args.ground_truth_exec.is_dir():
        test_dir: Path = args.ground_truth_exec
        # From now on, any relative conversion from string to GitPath will be relative to the test directory!
        HACKY_RELPATH = test_dir

        spec_file = test_dir / 'spec.py'
        assert spec_file.exists(), "spec.py file not present!"

        # TODO: consider allowing the spec to import stuff instead of this hacky injection
        test_spec_dict = run_path(spec_file, init_globals=globals())
        test = test_spec_dict['SPEC']

        # TODO: add an option to declaratively-create the binary
        gt = BinFile(elf=GitPath(test_spec_dict['PRECOMPILED_ELF_FILE']))
    else:
        # Provide a generic case
        test = TestSpec(
            link_mode='static',
            links_picolibc=True,
            pic='unrestricted',
            extra_cflags=(),
            # Don't check anything
            unmodified_behavior=RunSpec(exit_code=None),
            replacement=ReplacementSpec(
                RunSpec(exit_code=123),
                replacements=[SectionReplacement(
                    bin_file='replacements/exit123.bin',
                    symbol_name='_exit',
                )],
            )
        )

        gt = BinFile.from_test(args.ground_truth_exec)

    assert not test.test_validation_sanity_check(gt, verbose=args.verbose)
    c, preliminary_score = test.score_symbolization(gt, args.symbolized_rel, verbose=args.verbose)

    if args.verbose:
        print("\n-- Segments and sections diff --")
        c.show_diff('elf', ['readelf', '-l'])
        c.show_diff('elf', ['readelf', '-S'])
