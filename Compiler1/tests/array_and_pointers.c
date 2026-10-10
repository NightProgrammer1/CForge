int main() {
    int arr[3];
    arr[0] = 100;
    arr[1] = 200;
    arr[2] = 300;

    int* ptr = &arr[0];
    ptr = ptr + 2;
    *ptr = 400;


    return arr[0] + arr[1] + arr[2];
}