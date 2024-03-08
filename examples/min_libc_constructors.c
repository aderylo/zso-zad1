static int STATIC_VAR = 1;

void __attribute__((constructor())) foo() {
    STATIC_VAR = 42;
}

int main() {
    return STATIC_VAR;
}
