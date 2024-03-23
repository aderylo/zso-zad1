from __future__ import annotations

import dataclasses
import logging
import re
import warnings
from enum import Enum
from operator import attrgetter
from pathlib import Path
from pprint import pformat
from tempfile import TemporaryDirectory
from typing import Optional, Union, Literal, NamedTuple, Iterable, Any, Sequence, Tuple


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


from .utils import merge_sorted
from .primitives import  strip_elf_exec_to_level, link_relocatable, run, check_call



# Make a modern Enum
dict_style_enum = ENUM_RELOC_TYPE_i386.copy()
del dict_style_enum['_default_']

RELOC_TYPE_I386 = Enum('RELOC_TYPE_I386', dict_style_enum)


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
        if not self.elf.exists():
            raise ValueError(f"ELF file {self.elf} doesn't exist.")
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
                        target_section_vaddr = self.resolve_section_vaddr(symbol.name, elf.get_section(sym_section_idx))
                        target = symbol['st_value'] + target_section_vaddr

                    reloc_spec = RelReloc(rel_vaddr, target, type_, symbol)
                    yield reloc_spec

    @staticmethod
    def resolve_section_vaddr(why: Relocation | Symbol | int | str, section):
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
