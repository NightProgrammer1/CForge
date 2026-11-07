// Tests global variables declaration, assignements, as function parameters.



//Global variable's initializer have to be compile time constant
const int a = 1;
const int b = 2;
const int c = 3;
const int d = 4;
const int e = 5;
const int f = 6;
const int g = 7;
const int h = 8;
const int i = 9;
const int j = 10;


// Global array declaration: all elements in initializer must be compile time constant
int arr[10] = { a,b,c,d,e,f,g,h,i,j };

int func(int* array)
{
	arr[0] = j;
	arr[1] = i;
	arr[2] = h;
	arr[3] = g;

	return *(array)+*(array + 9); // 10 + 10
}


int main()
{
	if (&arr) {
		return func(arr); // return 20
	}

}