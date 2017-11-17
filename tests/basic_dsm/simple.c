#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>


int test=0;
int comm_migrate(int nid);
int main(int argc, char* argv[])
{
	int migrate=0;
	if(argc>1)
		migrate=atoi(argv[1]);
	if(migrate)
	{
		printf("%d: before migrate, value is %d\n", getpid(), test);
		test=1;
		comm_migrate(1);
	}else
		printf("%d:after migrate, value is %d\n", getpid(), test);
	return 0;
}
