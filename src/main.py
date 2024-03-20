import lief
# ELF
binary = lief.parse("/home/zso/zso-zad1/statement/a.elf")


# print(binary.__doc__)
# print(binary.__module__)
# print(binary.__dir__())

# print(binary.header)

# fst_segment = binary.segments[0]
# print(fst_segment.__dir__())

# print(binary.segments[0])

def get_sections(binary):
    return binary.sections


content = list(binary.sections)[3].content

print(content[0])
