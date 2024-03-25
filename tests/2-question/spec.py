"""This is examples/question.c compiled with the provided Makefile. -no-plt -f-no-jump-tables, -O0."""
# Using Python files for now, it should be parsed from YAML

PRECOMPILED_ELF_FILE = 'question.elf'

SPEC = TestSpec(
    link_mode='static',
    links_picolibc=False,
    pic='simple',
    extra_cflags=(),
    unmodified_behavior=RunSpec(
        exit_code=42,
        stdin=None,
        expected_stdout='question.default-output',
    ),
    replacement=ReplacementSpec(
        RunSpec(
            exit_code=0,
            stdin=None,
            expected_stdout='question.replaced-output',
        ),
        replacements=[SectionReplacement(
            bin_file='/replacements/mock_simulated_getc_nodata.bin',
            symbol_name='simulated_getc',
        )],
    )
)
