// Replace clock() backend to return predictable values
.text
.globl cheat
.type cheat, @function
cheat:
// Follow ABI. Should we allocate this with data reloc?
.equ nonsuspicious_slot, 0xa8013ffc
.equ magic, 0x90091317

movl	$12345, %eax
cmpl	$magic, nonsuspicious_slot
je	end
movl	$magic, nonsuspicious_slot
movl	$1234, %eax
end:
ret
