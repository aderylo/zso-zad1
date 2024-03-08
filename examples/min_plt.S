// LD will generate @plt stubs only for unresolved symbols. This can be done in two ways:
// a) generate JUMP_SLOTS in .rel.dyn, emit stubs with "f_unbound" and put it as default entries.
//    these will reside in .plt
// b) generate simple GLOB_DAT relocations in .rel.dyn for .got and no "f_unbound" stub
//    if the slot has to be eagerly bound (i.e., there is GOT32 reloc for this symbol for instance by checking if defined)
//    these will reside in .plt.got
.type	lazy_jump_slot, @function
.weak lazy_jump_slot // not truly weak, as it needs to be resolved by the dynamic linker

.type	eager_glob_dat, @function
.weak eager_glob_dat // 0 if not resolved by a linker

.text
.section .text.startup.enter
.globl _start
_start:
call _l1
_l1:
popl %ebx
addl $_GLOBAL_OFFSET_TABLE_+(.-_l1), %ebx
// "static dynamic linker"
// Typically these are used for static-pie: static executables that need to support ASLR (they relocate themselves)
// But we may imagine a way to build a single binary, that will resolve a static function
// depending on the machine support.
call relocate

cmpl $0x0, eager_glob_dat@GOT(%ebx)
je   skip
call eager_glob_dat@PLT

skip:
// This is resolved to a jump thunk
call lazy_jump_slot@PLT
// This is collapsed into PC32
call _exit@PLT
int $0x3

// Static binaries (e.g. -static-pie) should be able to relocate themselves.
// But with such an option LD will resolve all symbols.
// We don't need an external loader as the ABI is straightforward,
// and we just need to register a handler, so that the first three words of GOT are:
// [&_DYNAMIC, void* loader_any, resolve_func]
.type	relocate, @function
relocate:
// Our data and handler.
movl $0xbacababa, 4(%ebx)
movl find@GOT(%ebx), %edx
movl %edx, 8(%ebx)
ret

.type	find, @function
find:
// Arguments for relocation: func slot id; loader_any, GOT in %ebx.
// This is using the stack for argument passing to not destroy any registers.
pop %eax
pop %eax // use the offset as fake return value
// Typically we would replace a slot in GOT and jump there.
ret

.section	.rodata,"a"
.align 4
.type	retval, @object
.size	retval, 4
retval:
	.long	42
