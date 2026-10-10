int main() {
    int zero_val = 0;
    int pos_val = 99;
    int result = 0;

    if (!zero_val) {
        result = result + 10; // Should execute (!0 is true)
    }

    if (!pos_val) {
        result = result + 100; // Should NOT execute (!99 is false)
    }

    // Expected result: 10.
    return result;
}