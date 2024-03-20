from capstone import *

def read_segment_and_disassemble(file_path, offset, size):
    # Open the file in binary read mode
    with open(file_path, 'rb') as f:
        # Move the file pointer to the specified offset
        f.seek(offset)
        # Read the specified size of the segment
        segment = f.read(size)
    
    # Prepare for disassembly (assuming x86 64-bit architecture here)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    
    # Disassemble the segment
    for instruction in md.disasm(segment, offset):
        print("0x%x:\t%s\t%s" % (instruction.address, instruction.mnemonic, instruction.op_str))

# Example usage:
file_path = '/home/zso/zso-zad1/statement/a.elf'  # Replace 'path_to_your_file' with the actual file path
offset = 0x00100e # Example offset
size = 0x0001ba  # Example size

print("----\n")
read_segment_and_disassemble(file_path, offset, size)
