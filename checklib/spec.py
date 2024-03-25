#!/usr/bin/env python3
from __future__ import annotations

import dataclasses
import logging
import shutil
import traceback

from pathlib import Path
from typing import Optional, Union, Literal, NamedTuple, Iterable, Any, Sequence, Tuple

from elftools.elf.elffile import ELFFile


from .utils import GitPath
from .primitives import execute, remove_section, replace_section
from .elf import BinFile, Comparator

logger = logging.getLogger(__name__)

__all__ = ['SectionReplacement', 'ReplacementSpec', 'RunSpec', 'OverrideFromObject', 'TestSpec']

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


@dataclasses.dataclass
class OverrideFromObject:
    """Normally when linking multiple files with same named sections, they are concatenated.

    The section names and symbols will be renamed to match the resulting one.
    Grouping, on the other hand, requires special handling.
    """
    obj_file: str

    def prepare(self, c: Comparator) -> Path:
        raise NotImplementedError("Not yet implemented fully")



@dataclasses.dataclass
class RunSpec:
    exit_code: Optional[int]
    #: Path to file
    expected_stdout: Optional[str] = None
    stdin: Optional[str] = None

    def compare_output(self, bin: BinFile) -> list[str]:
        errs = []
        result = execute(bin.elf, self.stdin and GitPath(self.stdin))
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
