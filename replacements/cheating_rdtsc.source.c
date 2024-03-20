long cheat() {
    static int first = 1;
    if (first) {
        first = 0;
        return 1234;
    } else {
        return 12345;
    }
}
