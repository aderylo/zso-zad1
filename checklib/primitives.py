from __future__ import annotations

import os
import shlex
import shutil
import sys
import typing
from functools import wraps
from pathlib import Path
from subprocess import check_call, CompletedProcess, run, DEVNULL
from typing import Optional, Union, Literal, Sequence

from elftools.elf.elffile import ELFFile

from .utils import GitPath

__all__ = [
    'run', 'check_call',
    'link_relocatable', 'strip_elf_exec_to_level',
    'replace_section', 'remove_section',
    'execute',
]

# Allow for externally hacking these vars
GCC_FOR_LD = os.environ.get('LD', 'x86_64-linux-gnu-gcc-10')
LD_BASE_FLAGS = ['-march=lakemont', '-mtune=lakemont', '-m32', '-miamcu', '-msoft-float',
                 '-nostdlib',  '-Wl,--no-relax', '-Wl,--emit-relocs',  '-Wl,--build-id=none',
                 # '-Wl,--no-warn-rwx-segments',
                 '-Wl,--strip-debug',
                 '-T', GitPath('/picolibc/iamcu/lib/picolibc.ld')
                 ]
LD_STATIC_FLAGS = ['-Wl,-znorelro,-ztext', '-static']

# GNU objcopy seems to improperly handle ET_EXEC with static relocation sections
OBJCOPY = os.environ.get('LLOBJCOPY', 'llvm-objcopy')
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
if not typing.TYPE_CHECKING:
    @wraps(run)
    def run(cmd, *args, **kwargs):
        if TRACE_COMMANDS:
            print(shlex.join(map(str, cmd)), file=sys.stderr)
        return run.__wrapped__(cmd, *args, **kwargs)

    @wraps(check_call)
    def check_call(cmd, *args, **kwargs):
        if TRACE_COMMANDS:
            print(shlex.join(map(str, cmd)), file=sys.stderr)
        return check_call.__wrapped__(cmd, *args, **kwargs)

def untrace_commands():
    global TRACE_COMMANDS
    TRACE_COMMANDS = False


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


def replace_section(src: Path, dst: Path, section_name: str, data_file: Path):
    check_call([OBJCOPY, '--update-section', f'{section_name}={data_file}', src, dst])

def remove_section(src: Path, dst: Path, section_name: str):
    check_call([OBJCOPY, '--remove-section', section_name, src, dst])


def execute(bin: Path, stdin: Optional[Path], **kwargs) -> CompletedProcess:
    # TODO: run in qemu
    # To make sure the program is not modifying it
    if stdin is not None:
        kwargs['input'] = stdin.read_bytes()
    else:
        kwargs['stdin'] = DEVNULL

    return run(['i386', bin if bin.is_absolute() else f"./{bin}"], capture_output=True, timeout=10, **kwargs)
