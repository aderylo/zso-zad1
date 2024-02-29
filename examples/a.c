#include "_syscalls_impl.h"

char putc(char c) {
    return (char) _putc(1, c);
}
char putc_err(char c) {
    return (char) _putc(2, c);
}
char getc() {return (char) _getc();}

typedef char(*pfunc)(char);
const pfunc ARR[2] = {putc, putc_err};

static int FOO = 1;

void foo() {
    pfunc arr[2] = {putc, putc_err};
    if (&_getc) arr[FOO]('F');
}

void bar() {
    static pfunc arr[2] = {putc, putc_err};
    if (&FOO) arr[FOO]('F');
}

int main() {
    FOO = 0;
    putc('a');
    putc('\n');
    char a = getc();
    switch (a) {
        case -1:
            putc_err('E');
            putc_err('O');
            putc_err('F');
            _exit(1);
        case 0 ... 5: putc('h');
        case ' '+1 ...'a': _exit(33);
        case 'b': return 20;
        default: break;
    }
    foo();
    bar();
    return 0;
}

void ENTRYPOINT _start() {
    _exit(main());
}
