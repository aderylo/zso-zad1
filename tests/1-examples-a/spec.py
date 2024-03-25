"""This is examples/a.c compiled with the provided Makefile. No GOT, -O0."""
# Using Python files for now, it should be parsed from YAML

PRECOMPILED_ELF_FILE = 'a.elf'

SPEC = TestSpec(
    link_mode='static',
    links_picolibc=False,
    pic='none',
    extra_cflags=(),
    unmodified_behavior=RunSpec(
        exit_code=0,
        stdin='a.input',
        expected_stdout='a.default-output',
    ),
    replacement=ReplacementSpec(
        RunSpec(
            exit_code=33,
            stdin='a.input',
            expected_stdout='a.replaced-output',
        ),
        replacements=[SectionReplacement(
            bin_file='/replacements/return0.bin',
            symbol_name='getc',
        )],
    )
)
