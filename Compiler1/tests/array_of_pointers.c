int main() {
    int a = 12;
    int b = 30;

    int* ptrs[2];

    ptrs[0] = &a;
    ptrs[1] = &b;

    return *ptrs[0] + *ptrs[1]; // Expected exit code: 42
}