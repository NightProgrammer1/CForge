


int increase_by_one();


int main()
{
	int i = 0;


	for (int a = 0; a != 10; a += increase_by_one())
	{

		i += 2;
	}

	return i;
}



int increase_by_one()
{
	return 1;
}
