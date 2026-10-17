int main() {
    int n = 5;
    int count = 0;

    // Loop continues as long as n is truthy (non-zero)
    while (n) {
        count = count + 1;
        n = n - 1; // Eventually hits 0, which terminates the loop
    }

    // count should equal 5
    if (count == 5) {
        return 10;
    }
    return 1;
}