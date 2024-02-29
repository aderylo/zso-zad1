#include "_syscalls_impl.h"

unsigned long long rdtsc (void)
{
    unsigned long long tick;
    asm (
            "rdtsc"
            : "=A"(tick)
            );
    return tick;
}

void __attribute__((noreturn)) _exit(int code) {
    asm (
            "int $0x80    \n"
            "hlt \n"
            :
            : "a" (1), "b" (code)
            : /* reserve */);
    for(;;);
}

int _getc() {
    char c;
    int not_eof;
    asm (
            "int $0x80    \n"
            : /* out */ "=a" (not_eof)
            : /* in */
                "a" (/* sys_read */ 3),  "b" (/* fd */ 0),
                "c" (&c), "d" (/* count */1)
            : "memory");
    if (not_eof) return c;
    else return -1;
}

int _putc(int fd, char c) {
    int not_eof;
    asm (
            "int $0x80    \n"
            : /* out */ "=a" (not_eof)
            : /* in */ "a" (/* sys_write */ 4), "b" (fd), "c" (&c), "d" (/* count */ 1)
            :);
    return not_eof;
}
