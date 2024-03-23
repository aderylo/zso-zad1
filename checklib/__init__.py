try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    raise ImportError("This script requires the pyelftools package to be installed.\n"
                      "Alternatively, try using the .pex self-contained version of this script.")
