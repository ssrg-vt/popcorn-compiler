#include <stdio.h>
#include <stdlib.h>

int f(int a)
{
	return a + 2;
}

int main()
{
	int x = f(2);
	printf("hello, world, %d\n", x);
	
	return 0;
}
