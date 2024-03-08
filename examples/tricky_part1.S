// TRICKY LITERAL
.text
.section .text.french_tail_callback
.globl french_tail_callback
french_tail_callback:
// args: eax: function to call that accepts a single string
movl %eax, %edx // edx is scratch
movl $.Lstring_latin1, %eax
jmp *%edx

.Lstring_latin1:
// One may see a function call in this "french" word.
.string "L\xe8t"
.skip 4

// --- JUMP TABLES
.section .text.func_jumptable
.globl func_jumptable
.type func_jumptable, @object
func_jumptable:
// @PLT MAY BE USED ONLY WITH .long AND NOT WITH .word
// But it is still relative, we need 32PLT reloc, but it's Sun-specific.
// Instead, let's make a 2-step jump table to funcs in PLT.
.long a
.long b
.long c
.long d

.type _func_jumptable, @function
_func_jumptable:
a:
// Weakref force employing weak symbols semantics for symbols statically present
// In essence, this won't be collapsed into PC32, but will create a JUMP_SLOT.
.weakref _div, div
jmp _div@PLT
.align 4
b:
jmp abs@PLT
.align 4
c:
jmp itoa@PLT
.align 4
d:
jmp utoa@PLT
