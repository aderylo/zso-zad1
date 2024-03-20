// Just return 0
.text
.globl cheat
.type cheat, @function
cheat:
xorl %eax, %eax
// In case we replace a func returning 8 bytes
xorl %edx, %edx
ret
