int main() {
    int outer = 0;
    int total = 0;

    while (outer < 3) {
        int inner = 0;
        while (inner < 4) {
            total++;
            inner++;
        }
        outer++;
    }

    return total; 
}