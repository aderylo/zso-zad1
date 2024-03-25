"""
A simple real code from Embench. The code is straightforward with no switches, structs or indirection.
It is compiled without PIC and with -O1.
"""
# Using Python files for now, it should be parsed from YAML

PRECOMPILED_ELF_FILE = 'libedn.elf'

SPEC = TestSpec(
    link_mode='static',
    links_picolibc=True,
    pic='none',
    extra_cflags=(),
    unmodified_behavior=RunSpec(
        exit_code=0,
    ),
    replacement=ReplacementSpec(
        RunSpec(
            exit_code=0,
        ),
        replacements=[SectionReplacement(
            bin_file='/replacements/return_for_libedn.bin',
            symbol_name='codebook',
        )],
    )
)
