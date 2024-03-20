#include <stdio.h>
#include <string.h>

int main() {
    char buf[256];
    puts("Say my name: ");
    gets(buf);
    puts(buf);
    if (strcasecmp("Heisenberg", buf) == 0) {
        puts("You're goddamn right!");
        return 0;
    } else {
        puts("WRONG!");
        return 42;
    }
}
