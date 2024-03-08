.text
.section .text.startup.enter,"ax"
.globl _start
_start:

// Local call (PC32), nonstandard register
call	__x86.get_pc_thunk.cx
// Another setup
addl	$_GLOBAL_OFFSET_TABLE_, %ecx

// indirect call
leal _exit@GOT(%ecx), %edx

// table offset v1
movl $7, %ebx
movl retval@GOTOFF(%ecx, %ebx, 4), %eax

// table offset v2 with addend
addl $retval@GOTOFF+4, %ecx
addl 4(%ecx), %eax

call *(%edx)
int $0x3

.section	.rodata,"a"
.align 4
.type	retvals, @object
.size	retvals, 32
retval:
.long	12
.long	22
.long	32
.long	42
.long	52
.long	62
.long	72
.long	82

// G means this section may be grouped (similarly to a weak symbol)
.section	.text.__x86.get_pc_thunk.cx,"axG",@progbits,__x86.get_pc_thunk.bx,comdat
.globl	__x86.get_pc_thunk.cx
.hidden	__x86.get_pc_thunk.cx
.type	__x86.get_pc_thunk.cx, @function
__x86.get_pc_thunk.cx:
movl	(%esp), %ecx
ret
