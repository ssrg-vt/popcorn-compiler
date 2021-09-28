#include <stdio.h>
#include <unistd.h>

#include "migrate.h"

int tid_main;

void func2(int i)
{
	printf("[%d] Executing %s, on %s node.\n", i, __func__,
			gettid()==tid_main?"local":"remote");
}

void func1(int i)
{
	printf("[%d] Executing %s, on %s node.\n", i, __func__,
			gettid()==tid_main?"local":"remote");
	sleep(5);
	func2(i);
}

int main(int argc, char *argv[])
{
	int i;

	tid_main = gettid();
	printf("thread id on x86 node %d.\n", tid_main);

	for (i = 0; i < 10; i++) {
		func1(i);
	}
	return 0;
}
