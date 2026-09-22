int main() {
    int arr[3];
    arr[0] = 5;
    arr[1] = 15;
    arr[2] = 25;

    int* ptr = &arr[2]; // Points to arr[2] (val 25)
    int val1 = *ptr;    // 25

    ptr = ptr - 1;      // Points to arr[1] (subtracts 4 bytes)
    int val2 = *ptr;    // 15

    ptr = ptr - 1;      // Points to arr[0] (subtracts 4 bytes)
    int val3 = *ptr;    // 5

    return val1 - val2 + val3; // Expected exit code: 15 (25 - 15 + 5)
}