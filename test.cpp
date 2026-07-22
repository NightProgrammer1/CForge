int func()
{
	int a = 30;
	int b = 20;
	if (a > b) {
		return 2;
	}
}

int main()
{
	int a = func();

	exit(a);
}
