#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <migrate.h>


int test=0;
int comm_migrate(int nid);
int main(int argc, char* argv[])
{
	int migrate=0;
	if(argc>1)
		migrate=atoi(argv[1]);
	else
		migrate=1;
	if(migrate)
	{
		printf("%d: before migrate\n", getpid());
		new_migrate(1);
		new_migrate(0);
		printf("%d: after migrate\n", getpid());
	}
	return 0;
}
