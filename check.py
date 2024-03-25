#!/usr/bin/env python3
from __future__ import annotations

import argparse
import logging
import os
import sys
from pathlib import Path
from runpy import run_path
from typing import Optional, Union, Literal, NamedTuple, Iterable, Any, Sequence, Tuple

from checklib.utils import GitPath, _set_relative_base
from checklib.elf import BinFile, Comparator
from checklib.primitives import untrace_commands, STRIP_LEVEL_FLAGS, strip_elf_exec_to_level, check_call
from checklib.spec import *

logger = logging.getLogger(__name__)

parser = argparse.ArgumentParser(description="Check the solution.", prog=os.environ.get('PEX'))
parser.add_argument('--quiet', '-q', action='store_false', dest='verbose')
subparsers = parser.add_subparsers(title='Subcommands', dest='subparser_name', required=True)

parser_single = subparsers.add_parser('check-symbolization',
                                      help="Validates the resymbolized file against a test/ground truth.")
parser_single.add_argument('symbolized_rel', type=Path)
parser_single.add_argument('ground_truth_exec', type=Path)

parser_strip = subparsers.add_parser('strip',
                                      help="Strip the binary to a specified level.")
parser_strip.add_argument('strip_input', type=Path)
parser_strip.add_argument('strip_output', type=Path)
parser_strip.add_argument('--level', choices=STRIP_LEVEL_FLAGS.keys(), default='strip')


parser_check = subparsers.add_parser('check',
                                      help="Check the solution on a single test.")
parser_check.add_argument('--level', choices=STRIP_LEVEL_FLAGS.keys(), default='strip')
parser_check.add_argument('--symbolizer', '-s', type=Path, default=GitPath('/solution/symbolize'), help='Default is in solution/symbolize')
parser_check.add_argument('test_dir', type=Path)
parser_check.add_argument('extra_args', type=str, nargs=argparse.REMAINDER, help="Passed directly to the symbolizer")

def check(args: NamedTuple):
    # This is currently a major hack
    assert args.test_dir.is_dir()
    test_dir: Path = args.test_dir

    # Strip part
    _test, elf = load_spec(test_dir)
    args.strip_input = elf
    args.strip_output = elf.with_suffix(elf.suffix + '.strip')
    strip(args)

    # Symbolizer part
    symbolized = elf.with_suffix('.symbolized')
    check_call([args.symbolizer, args.strip_output, symbolized] + args.extra_args)

    # Check part
    args.symbolized_rel = symbolized
    args.ground_truth_exec = test_dir
    return check_single(args)
parser_check.set_defaults(subcommand_func=check)

def load_spec(test_dir: Path) -> Tuple[TestSpec, Path]:
    # From now on, any relative conversion from string to GitPath will be relative to the test directory!
    _set_relative_base(test_dir)

    spec_file = test_dir / 'spec.py'
    assert spec_file.exists(), "spec.py file not present!"

    # TODO: consider allowing the spec to import stuff instead of this hacky injection
    test_spec_dict = run_path(spec_file, init_globals=globals())
    test = test_spec_dict['SPEC']

    # TODO: add an option to declaratively-create the binary
    elf = GitPath(test_spec_dict['PRECOMPILED_ELF_FILE'])
    return test, elf

def check_single(args: NamedTuple):
    if args.ground_truth_exec.is_dir():
        test, elf = load_spec(args.ground_truth_exec)
        gt = BinFile(elf=elf)
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

    if args.verbose:
        print("Test validation...")
    assert not test.test_validation_sanity_check(gt, verbose=args.verbose), "Test validation failed!"
    c, preliminary_score = test.score_symbolization(gt, args.symbolized_rel, verbose=args.verbose)

    if args.verbose:
        print("\n-- Segments and sections diff --")
        c.show_diff('elf', ['readelf', '-l'])
        c.show_diff('elf', ['readelf', '-S'])
    print(f"Preliminary score (up to change with the script update, read the produced notices): {preliminary_score:.3}")

parser_single.set_defaults(subcommand_func=check_single)

def strip(args: NamedTuple):
    strip_elf_exec_to_level(args.strip_input, args.strip_output, args.level)
parser_strip.set_defaults(subcommand_func=strip)

if __name__ == '__main__':
    args = parser.parse_args()

    if args.verbose is False:
        untrace_commands()

    sys.exit(args.subcommand_func(args))
