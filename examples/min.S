.text
.section .text.startup.enter
.globl _start
.globl _exit
_start:
movl retval, %eax
call _exit
int $0x3


.section	.rodata,"a"
.align 4
.type	retval, @object
.size	retval, 4
retval:
	.long	33
