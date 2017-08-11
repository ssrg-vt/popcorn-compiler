#include <stdio.h>
#include <stdlib.h>

#include "test.h"

int f(int a)
{
	return my_function(a, a);
}

int main()
{
	int x = f(2);
	printf("hello, world, %d\n", x);
	
	return 0;
}
