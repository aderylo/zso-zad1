# Using Python files for now, it should be parsed from YAML

PRECOMPILED_ELF_FILE = 'min.elf'

SPEC = TestSpec(
    link_mode='static',
    links_picolibc=False,
    pic='none',
    extra_cflags=(),
    unmodified_behavior=RunSpec(exit_code=33),
    replacement=ReplacementSpec(
        RunSpec(
            exit_code=123,
            expected_stdout=None
        ),
        replacements=[SectionReplacement(
            bin_file='/replacements/exit123.bin',
            symbol_name='_exit',
        )],
    )
)
