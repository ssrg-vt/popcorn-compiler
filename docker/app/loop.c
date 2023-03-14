#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "migrate.h"

int pid_main;

void func2(int i)
{
	printf("[%d] Executing %s on: ", i, __func__);
	fflush(stdout);
	system("uname -m");
}

void func1(int i)
{
	printf("[%d] Executing %s on: ", i, __func__);
	fflush(stdout);
	system("uname -m");
	sleep(1);
	func2(i);
	sleep(3);
}

int main(int argc, char *argv[])
{
	int i;

	pid_main = getpid();
	printf("pid on x86 node %d.\n", pid_main);

	for (i = 0; i < 1000; i++) {
		func1(i);
	}
	return 0;
}
