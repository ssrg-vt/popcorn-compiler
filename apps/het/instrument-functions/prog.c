#include <stdio.h>
#include <stdlib.h>

extern void sys_msleep(int ms);

void f(void) {
	printf("Iterate!\n");
}

int main(int argc, char *argv[])
{
	for(int i=0; i<10; i++) {
		sys_msleep(1000);
		f();
	}

	return 0;
}
