#!/usr/bin/env python3
from __future__ import annotations

import argparse
import dataclasses
import logging
import traceback
from pathlib import Path
from pprint import pformat
from runpy import run_path
from typing import Optional, Union, Literal, NamedTuple, Iterable, Any, Sequence, Tuple

from checklib.utils import GitPath, _set_relative_base
from checklib.elf import BinFile, Comparator
from checklib.spec import *

logger = logging.getLogger(__name__)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Check the solution.")
    parser.add_argument('symbolized_rel', type=Path)
    parser.add_argument('ground_truth_exec', type=Path)
    parser.add_argument('--quiet', '-q', action='store_false', dest='verbose')
    args = parser.parse_args()

    if args.verbose is False:
        primitives.untrace_commands()

    # TODO: load this from file
    if args.ground_truth_exec.is_dir():
        test_dir: Path = args.ground_truth_exec
        # From now on, any relative conversion from string to GitPath will be relative to the test directory!
        _set_relative_base(test_dir)

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
