import lief
import copy


binary = lief.parse("/home/zso/zso-zad1/examples/min_libc.elf")


for section in binary.sections:
  print(section.name) # section's name
  print(section.size) # section's size
  print(len(section.content)) #
