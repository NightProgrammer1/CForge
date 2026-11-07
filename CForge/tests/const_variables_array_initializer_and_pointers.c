// Tests constant variables with array initializer

int func(const int* v) // accept an int* or const int*
{
	//v cannot be modified here
	//*v = 10;
	return *v;
}

int main() {
	const int c1 = 10;
	int m1 = 20;
	const int c2 = 30;

	// Should successfully normalize {const int, int, const int} into an int[3] array type
	int my_array[3] = { func(&c1), m1, c2 };

	return my_array[0]; // returns 10
}