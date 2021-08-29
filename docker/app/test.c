#include <stdio.h>
#include <unistd.h>

#include "migrate.h"

int pid_main;

void func2(int i)
{
	printf("[%d] Executing %s, %s.\n", i, __func__,
			getpid()==pid_main?"locally":"on remote node");
}

void func1(int i)
{
	printf("[%d] Executing %s, %s.\n", i, __func__,
			getpid()==pid_main?"locally":"on remote node");
	sleep(2);
	func2(i);
}

int main(int argc, char *argv[])
{
	int i;

	pid_main = getpid();
	printf("pid on x86 node %d.\n", pid_main);

	for (i = 0; i < 10; i++) {
		func1(i);
	}
	return 0;
}
