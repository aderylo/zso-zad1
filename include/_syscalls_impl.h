#ifndef _SYSCALLS_IMPL
#define _SYSCALLS_IMPL

#define ENTRYPOINT __attribute__((section(".text.init.enter"), used))
unsigned long long rdtsc (void);
void __attribute__((noreturn)) _exit(int code);
int _getc();
int _putc(int fd, char c);

#endif
