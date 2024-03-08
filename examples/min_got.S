.text
.section .text.startup.enter
.globl _start
_start:
call _l1
_l1:
popl %ebx
addl $_GLOBAL_OFFSET_TABLE_+(.-_l1), %ebx

movl retval@GOTOFF(%ebx), %eax
call *_exit@GOT(%ebx)
int $0x3

.section	.rodata,"a"
.align 4
.type	retval, @object
.size	retval, 4
retval:
	.long	42
