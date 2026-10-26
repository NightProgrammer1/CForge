int add(int a, int b) {
    return a + b;
}

int main() {
    // Stack-allocated array & indexing
    int arr[2];
    arr[0] = 10;
    arr[1] = 20;

    // Function call & basic arithmetic
    int res = add(arr[0], arr[1]);

    // Conditional branching (if statement)
    if (res > 25) {
        res = res + 5;
    }

    // Expected Exit Code: (10 + 20) + 5 = 35
    return res;
}