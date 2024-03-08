#include <stdio.h>
#include <string.h>
#include <iconv.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
extern void french_tail_callback(void (*func) (char*));

static char buf[32];
static void puts_latin1_as_utf8(char *str) {
    iconv_t  cd = iconv_open("UTF-8", "ISO-8859-1");
    if (cd == (iconv_t) -1) {
        puts("OPEN ERROR");
        return;
    }
    size_t insize = strlen(str);
    size_t outsize = sizeof(buf);
    char *out = buf;
    size_t res = iconv(cd, &str, &insize, &out, &outsize);
    if (res == (size_t) -1) {
        puts("ERROR");
    } else {
        puts(buf);
    }
    iconv_close(cd);
}

extern char* (*func_jumptable[])(int, char*, int);

int main() {
#ifdef FULL_TEST_VERSION
    french_tail_callback(puts_latin1_as_utf8);
#else
    // You need to guess the encoding properly
    // examples/tricky.elf | iconv -f iso88591
    french_tail_callback(puts);
#endif

    char local_buf[32];
    puts(func_jumptable[2](-123, local_buf, 10));
    return 47;
}

#pragma GCC diagnostic pop
