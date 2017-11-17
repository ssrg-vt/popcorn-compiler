#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>


int test=0;
int new_migrate(int nid);
int main(int argc, char* argv[])
{
	test=1;
	printf("%d: before migrate, value is %d\n", getpid(), test);
	new_migrate(1);
	//migrate(1,NULL,NULL);
	printf("%d:after migrate, value is %d\n", getpid(), test);
	return 0;
}
