#include <stdio.h>
#include <sys/cdefs.h>
#include "_syscalls_impl.h"

// --oslib=myos
/* Picolibc part */
static int simulated_putc(char c, FILE *file)
{
    int written;
    (void) file;
    written = _putc(1, c); /* Defined by underlying system */
    if (!written) return EOF;
    return c;
}

static int simulated_getc(FILE *file)
{
    unsigned char c;
    (void) file; /* Not used in this function */
    c = _getc(); /* Defined by underlying system */
    if (c < 0)
        return EOF;
    return c;
}

static FILE __stdio = FDEV_SETUP_STREAM(
        simulated_putc,
        simulated_getc,
        NULL,
        _FDEV_SETUP_RW);
FILE *const stdin = &__stdio;
__strong_reference(stdin, stdout);
__strong_reference(stdin, stderr);
