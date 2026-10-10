


int increase_by_one()
{
	return 1;
}

int main()
{
	int i = 0;


	for (int a = 0; a != 10; a += increase_by_one())
	{

		i++;
	}

	return i;
}


