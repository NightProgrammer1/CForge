int main() {
    int x = 10;
    int result = 0;

    if (x == 5) {
        result = 1;
    }
    else if (x == 10) {
        result = 2; // Should hit this
    }
    else {
        result = 3;
    }

    // Expect result == 2 -> exit 0
    return result - 2;
}