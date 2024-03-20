from capstone import *
import lief

def dissassemble(data):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    return md.disasm(data, 0)

def print_asm(instruction_lst):
    for instruction in instruction_lst:
        print("0x%x:\t%s\t%s" % (instruction.address, instruction.mnemonic, instruction.op_str))


if __name__ == "__main__":
    binary = lief.parse("/home/zso/zso-zad1/statement/a.elf")

    for section in binary.sections:
        if section.name == ".text":
            print(bytes(section.content))
            dsm = dissassemble(bytes(section.content))
            print_asm(dsm[0])

