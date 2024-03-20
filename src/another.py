import lief

# Create a new ELF binary
from lief import elf



binary = ELF.parse("../examples/c.elf")


# Set the type of the binary (EXECUTABLE, SHARED_OBJECT, etc.)
binary.header.file_type = 

# Set the machine architecture (x86, ARM, etc.)
binary.header.machine_type = lief.ELF.ARCH.x86_64

# Create a new section
section = lief.ELF.Section()
section.name = "my_section"
section.type = lief.ELF.SECTION_TYPES.PROGBITS
section.content = [0x90] * 100  # NOP sled for example
section.add(lief.ELF.SECTION_FLAGS.EXECINSTR | lief.ELF.SECTION_FLAGS.ALLOC)

# Add the section to the binary
binary.add(section)

# Optionally, you can add an entry point to the binary
binary.entrypoint = 0x1000

# Save the binary to an ELF file
binary.write("my_new_elf_file.elf")


