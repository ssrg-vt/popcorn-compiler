#include <stdio.h>
#include <unistd.h>

#include "migrate.h"

int tid_main;

void func(int i)
{
	printf("[%d] (thread %d): Executing %s, on %s node.\n", i, gettid(),
		__func__, gettid()==tid_main?"local":"remote");
	sleep(2);
}

int main(int argc, char *argv[])
{
	int i;

	tid_main = gettid();
	printf("[+] ping pong hopping between two nodes\n");
	printf("[+] thread id on x86 node %d.\n", tid_main);

	for (i = 0; i < 10; i++) {
		func(i);
	}
	return 0;
}
