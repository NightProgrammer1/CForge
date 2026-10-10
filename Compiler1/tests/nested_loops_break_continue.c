int main() {
    int total = 0;

    for (int i = 0; i < 5; i = i + 1) {
        if (i == 2) {
            continue; // Skip i = 2
        }

        for (int j = 0; j < 5; j = i + 1) {
            if (j == 3) {
                break; // Break out of inner loop when j = 3
            }
            total = total + 1;
        }
    }

    // i runs for 0, 1, 3, 4 (4 outer iterations)
    // j runs for 0, 1, 2 (3 inner iterations per outer)
    // total = 4 * 3 = 12
    if (total == 12) {
        return 10;
    }
    return 1;
}