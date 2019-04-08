#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/user.h>
#include <sys/reg.h>

#define MIGRATION_GBL_VARIABLE "__migrate_gb_variable"
static char* get_binary_path(int pid)
{
        ssize_t ret;
        #define MAXPATH 2048
        static char binary_path[MAXPATH];
        static char exe_path[64];

        sprintf(exe_path, "/proc/%d/exe", pid);
        //pr_info("%s: proc exec path is %s\n", __func__, exe_path);
        printf("%s: proc exec path is %s\n", __func__, exe_path);
        ret = readlink(exe_path, binary_path, MAXPATH);
        if(ret<=0)
                return NULL;
        binary_path[ret]='\0';
        return binary_path;
}

const int long_size = sizeof(int);
long getdata(pid_t child, long addr)
{   
	return ptrace(PTRACE_PEEKDATA, child, addr, NULL);
}
void putdata(pid_t child, long addr, long data)
{
	printf("putdata addr pid %d %lx data %ld\n", child, addr, data);
	ptrace(PTRACE_POKEDATA, child, addr, (void*) data);
}

long get_sym_addr(char* bin_file, char* sym)
{
	FILE *fp;
	char buff[256];
	char cmd[128];

	/* Open the command for reading. */
	sprintf(cmd, "/usr/bin/nm %s", bin_file);
	fp = popen(cmd, "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	char* addr_str=NULL;
	char* type;
	char* name;
	/* Read the output a line at a time - output it. */
	while (fgets(buff, sizeof(buff)-1, fp) != NULL) {
		addr_str = strtok(buff, " ");
		type = strtok(NULL, " ");
		name = strtok(NULL, " ");
		if(name && strncmp(sym, name, strlen(sym))==0)
			break;
	}

	/* close */
	pclose(fp);

	if(addr_str)
		return strtol(addr_str, NULL, 16);
	return -1;
}

int main(int argc, char *argv[])
{   
	pid_t traced_process;
	char *target_arch;
	siginfo_t si;
	long ret_data;
	long addr;
	int ret;
	if(argc != 3) {
		printf("Usage: %s pid arch\n",
		   argv[0]);
		exit(1);
	}
	/* args*/
	traced_process = atoi(argv[1]);
	target_arch = argv[2];
	char* bin_path = get_binary_path(traced_process);
	addr = get_sym_addr(bin_path, MIGRATION_GBL_VARIABLE);
	int pid=traced_process;

	/* attach */
	if(ptrace(PTRACE_SEIZE, traced_process, NULL, NULL)==-1)
	{
		perror("attach");
		exit(1);
	}
	
	ret = ptrace(PTRACE_INTERRUPT, pid, NULL, NULL);
	if (ret < 0) {
		perror("interrupt");
		exit(1);
	}

	//wait(NULL);

	int first=1;

	/* Wait stack transformation */
	while(1)
	{
		int status=0;
		ret=waitpid(pid, &status, __WALL);
		if (ret < 0){ // || WIFEXITED(status) || WIFSIGNALED(status)) {
			perror("error waitpid\n");
			goto err;
		}
		
		if (WIFEXITED(status)) {
                	printf("SEIZE %d: task exited normally\n", pid);
			exit(-1);

		}
		if (WIFSIGNALED(status)) {
			int sig=WTERMSIG(status);
			printf("-->killed by signal %d\n", sig);
                	printf("SEIZE %d: task exited\n", pid);
			exit(-1);
		}
		
		
		if (!WIFSTOPPED(status)) {
                	printf("SEIZE %d: task not stopped after seize\n", pid);
			exit(-1);
		}

		if(first)
		{
			printf("The process stopped a first time %d, %lx\n", traced_process, addr);

			/* Put one in the variable */
			putdata(traced_process, addr, 0);
			//sleep(2);
			//ret_data = getdata(traced_process, addr);
			//printf("ret data %ld\n", ret_data);

			int sig=0;
			if(WIFSTOPPED(status))
			{
				sig=WSTOPSIG(status);
				printf("stopped by signal %d\n", sig);
				if(sig!=SIGTRAP)
					goto err;
			}
			ret = ptrace(PTRACE_GETSIGINFO, pid, NULL, &si);
			if (ret < 0) {
				printf("SEIZE %d: can't read signfo", pid);
				exit(1);
			}
			printf("cont with sig %d\n", 0);
			/* Cont. for the process to do stack transformation */
			if (ptrace(PTRACE_CONT, pid, NULL, NULL)) {
				printf("Can't continue signal handling, aborting");
				goto err;
			}
			printf("cont sent with sig %d\n", 0);
			printf("going to wait %d\n", 0);
			first=0;
			continue;
		}
		ret = ptrace(PTRACE_GETSIGINFO, pid, NULL, &si);
		if (ret < 0) {
			printf("SEIZE %d: can't read signfo", pid);
			exit(1);
		}
		if(si.si_signo == SIGALRM && (ret_data = getdata(traced_process, addr))==-1){
			break;
		}else{ //if (PTRACE_SI_EVENT(si.si_code) != PTRACE_EVENT_STOP) {
			/*
			 * Kernel notifies us about the task being seized received some
			 * event other than the STOP, i.e. -- a signal. Let the task
			 * handle one and repeat.
			 */

			if (ptrace(PTRACE_CONT, pid, NULL,
						(void *)(unsigned long)si.si_signo)) {
				printf("Can't continue signal handling, aborting");
				exit(1);
			}
			continue;
		}
	}

	printf("ret data %ld\n", ret_data);
	ptrace(PTRACE_DETACH, traced_process,
	   NULL, NULL);
	return 0;
err:
	exit(-1);
}
