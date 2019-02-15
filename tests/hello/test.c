#include <stdio.h>
#include <unistd.h>

void print_iteration()
{
	static int i = 0;
	printf("[%d] iteration %d\n", getpid(), i++);
}

int main(int argc, char *argv[])
{

	while(1) {
		print_iteration();
		sleep(1);
	}

	return 0;
}
