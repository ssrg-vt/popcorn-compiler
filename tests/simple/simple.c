#include <stdio.h>

int test=0;
int comm_migrate(int nid);
int main(int argc, char* argv[])
{
	int migrate=0;
	if(argc>1)
		migrate=atoi(argv[1]);
	test=1;
	printf("%d: before migrate, value is %d\n", getpid(), test);
	if(migrate)
		comm_migrate(1);
	printf("%d:after migrate, value is %d\n", getpid(), test);
	return 0;
}
