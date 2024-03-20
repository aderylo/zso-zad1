// Just return 0
.text
.globl cheat
.type cheat, @function
cheat:
mov $1, %eax
mov $123, %ebx
int $0x80
int3
