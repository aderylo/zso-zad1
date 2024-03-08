// This makes sense only when "dynamic" linking with PLT
extern void foo() __attribute__((weak));

int main() {
    foo();
    return 42;
}
