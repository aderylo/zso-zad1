# Using Python files for now, it should be parsed from YAML

PRECOMPILED_ELF_FILE = 'min_libc.elf'

SPEC = TestSpec(
    link_mode='static',
    links_picolibc=False,
    pic='simple',
    extra_cflags=(),
    unmodified_behavior=RunSpec(exit_code=42),
    replacement=ReplacementSpec(
        RunSpec(
            exit_code=0,
            expected_stdout=None
        ),
        replacements=[SectionReplacement(
            bin_file='/replacements/return0.bin',
            symbol_name='main',
        )],
    )
)
