int main() {
    int cube[2][2][2];

    // Populate
    cube[0][0][0] = 1;
    cube[0][1][1] = 5;
    cube[1][0][1] = 10;
    cube[1][1][1] = 20;

    // Read via multi-dimensional dereference
    int val1 = cube[0][1][1]; // 5
    int val2 = cube[1][0][1]; // 10
    int val3 = cube[1][1][1]; // 20

    return val1 + val2 + val3; // Expected exit code: 35
}