// Just return 0
.text
.globl cheat
.type cheat, @function
cheat:
// eax is FILE*, we should be at 8
// we replace this slot: one simple hax is to change the address for each char
// it may either point to a trampoline or to a nop-slide + string table as here
// the nop slide is easier to generate
.rept 128
.byte 0x90
.endr

// update the offset
addl $1, 8(%eax)
// get offset of call
movl 8(%eax), %ecx

// return the byte extended
movsbl (string-cheat-1)(%ecx), %eax
ret

string:
.ascii "Heisenberg"
string_end:
.rept 128 - (string_end-string)
.byte 0xff
.endr
