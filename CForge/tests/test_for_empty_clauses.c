int main() {
    int i = 0;
    int count = 0;

    // Infinite-style loop with internal break condition
    for (;;) {
        if (i >= 5) {
            break;
        }
        count = count + 1;
        i = i + 1;
    }

    if (count == 5) {
        return 10; // should return here
    }
    return 1;
}