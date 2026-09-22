int sum_array(int* ptr, int size) {
    int total = 0;
    int i = 0;
    while (i < size) {
        total = total + *(ptr + i);
        i = i + 1;
    }
    return total;
}

int main() {
    int numbers[3];
    numbers[0] = 3;
    numbers[1] = 7;
    numbers[2] = 11;

    return sum_array(numbers, 3); // Expected exit code: 21
}