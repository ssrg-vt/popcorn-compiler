#include <stdio.h>

int foo(int a, int b)
{
	return a + b;
}

int main()
{
	int res = foo(1, 2);
	printf("res: %d\n", res);
	return 0;
}
