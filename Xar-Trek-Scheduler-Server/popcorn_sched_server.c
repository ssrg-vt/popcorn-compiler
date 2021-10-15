// gcc -O3 -Wall -I./ ./popcorn_sched_server.c ./mytimer.c -lpthread -o pop_server

/*
 * Popcorn scheduler server for FPGA
 * Copyright (C) 2020 Ho-Ren (Jack) Chuang <horenc@vt.edu>
 *
 * Distributed under terms of the MIT license.
 */


 #include "mytimer.h"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/syscall.h>

#include <signal.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define POPCORN_X86 "10.1.1.45" /* TODO - change it according to your setup */
#define POPCORN_ARM "10.1.1.51" /* TODO - change it according to your setup */


#define PORT "3490"  /* the port users will be connecting to */
#define BACKLOG 128  /* how many pending connections (applications) queue will hold */
#define MAXDATASIZE 128

#define POPCORN_NUM_PLATFORMS 3 /* x86, arm, fpga */

int running_tsks = 0;
int cpu_work_load = -1;
int status;

char KNL_HW_Mod_XCLBIN[10][100];
int kernel_qt;



/***
 * Compile: gcc -o popcorn_sched_server popcorn_sched_server.c
 */

/* Add more info/data structure to maintain the runqueue */
void update_runqueue(char *command, int pid)
{
	if (!strcmp(command, "END")) { /* process end */
		running_tsks--;
	} else {
		running_tsks++;
	}
	printf("- runqueue has %d tasks running-\n", running_tsks);
}

/* Set local migration flag in a proess via signals */
int set_migration(int pid, int sig_id)
{
	return kill(pid, sig_id);
}

/* Popcorn scheduling policy */
void schedule(char *command, int pid, char prog_name[MAXDATASIZE])
{

FILE *fp;
//char arm_tsh_load[10], fpga_kernel[50], fpga_tsh_load[10], cmd[100];

char fpga_kernel[50], cmd[100], cmd_str[50];

int arm_tsh_load, fpga_tsh_load, kernel_in_xclbin, kernel_busy;
kernel_in_xclbin = 0;
kernel_busy = 0;
int i;

	/* update runqueue info */
	update_runqueue(command, pid);
//	printf("TODO - SCHEDULING POLICY BEGINS HERE\n");
	if (!strcmp(command, "END")) { /* process end */
		return;
	}
   strcpy(cmd,"grep -E '\\s*" );
   strcat(cmd,prog_name );
   strcat(cmd, "\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f2 -d,");
//   printf("CMD= %s\n", cmd);
   fp = popen(cmd,"r");
   fscanf(fp,"%s", fpga_kernel);
   status = pclose(fp);
   
/*   
   strcpy(cmd,"xbutil query|grep -E '\\s*CU\\[\\s*.+\\]:.+:'|cut -f2 -d:| grep " );
   strcat(cmd,fpga_kernel );
   strcat(cmd, "|wc -l");
   printf("CMD= %s\n", cmd);   
   fp = popen(cmd,"r");
   fscanf(fp,"%s", cmd_str);
   kernel_in_xclbin = atoi (cmd_str);
   status = pclose(fp);
*/

    for(i = 1; i <= kernel_qt; i++) {

    if (!strcmp(fpga_kernel,KNL_HW_Mod_XCLBIN[i]))
    	{
	kernel_in_xclbin = 1;
	break;	
//	printf("YES(%d)\n",i);
//    	printf ("KNL_HW_Mod_XCLBIN[%d]=%s\n",i,KNL_HW_Mod_XCLBIN[i]);
	}

    }

   printf ("\nCPU LOAD = %d;", cpu_work_load);
   printf("Program = %s\n", prog_name);



   printf("FPGA KERNEL = %s(%d)\n", fpga_kernel, i);
   
   strcpy(cmd,"grep -E '\\s*" );
   strcat(cmd,prog_name );
   strcat(cmd, "\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f3 -d,");
   fp = popen(cmd,"r");
   fscanf(fp,"%s", cmd_str);
   fpga_tsh_load = atoi (cmd_str);
   status = pclose(fp);
   printf("FPGA TSH = %d; ", fpga_tsh_load);



   strcpy(cmd,"grep -E '\\s*" );
   strcat(cmd,prog_name );
   strcat(cmd, "\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f4 -d,");
   fp = popen(cmd,"r");
   fscanf(fp,"%s", cmd_str);
   arm_tsh_load = atoi (cmd_str);
   status = pclose(fp);
   printf("ARM TSH = %d\n", arm_tsh_load);


//   fpga_tsh_load = 10;
//   arm_tsh_load = 10;

	int ret;
	
/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
          NO	                 YES	               YES	            YES  --> x86 */
if (!(cpu_work_load > arm_tsh_load) && (cpu_work_load > fpga_tsh_load) &&  
	(kernel_in_xclbin == 1) &&  (kernel_busy == 1) ) 

	{
	printf("\t migrate pid %d to *** X86 *** (stay)\n", pid);
	ret = set_migration(pid, SIGRTMIN);

	printf ("1 - x86\n");
	}


/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
          YES	                 YES	                YES	            YES  --> ARM */
if ((cpu_work_load > arm_tsh_load) && (cpu_work_load > fpga_tsh_load) &&  
	(kernel_in_xclbin == 1) &&  (kernel_busy == 1) ) 
	
	{
	printf("\t migrate pid %d to *** ARM *** ret: %d ->\n", pid, ret);
 	ret = set_migration(pid, SIGUSR1);
	
	printf ("2 - ARM\n");
	}


/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
          NO	                 YES	                NO 	            X  --> x86+REC */
if (!(cpu_work_load > arm_tsh_load) && (cpu_work_load > fpga_tsh_load) &&  
	(kernel_in_xclbin == 0)                        ) 
	
	{
	printf("\t migrate pid %d to *** X86 *** (stay)\n", pid);
	ret = set_migration(pid, SIGRTMIN);

	printf ("3 - x86+REC\n");

	strcpy(cmd,"./xclbin_prog.sh KNL_2B.xclbin" );
   	fp = popen(cmd,"r");
   	status = pclose(fp);

	printf ("3 - x86+REC\n");


		}


/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
          YES	                 YES	                NO	            X  --> ARM+REC */
if ((cpu_work_load > arm_tsh_load) && (cpu_work_load > fpga_tsh_load) &&  
	(kernel_in_xclbin == 0)                      ) 

	{
	printf("\t migrate pid %d to *** ARM *** ret: %d ->\n", pid, ret);
 	ret = set_migration(pid, SIGUSR1);
		
	printf ("4 - ARM+REC\n");
	}


/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
          NO	                 NO	               	X                   X  --> x86 */
if (!(cpu_work_load > arm_tsh_load) && !(cpu_work_load > fpga_tsh_load)   
	                                                 ) 

	{
	printf("\t migrate pid %d to *** X86 *** (stay)\n", pid);
	ret = set_migration(pid, SIGRTMIN);
	
	printf ("5 - x86\n");
	}


/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
          YES	                 NO	                X	            X  --> ARM */
if ((cpu_work_load > arm_tsh_load) && !(cpu_work_load > fpga_tsh_load)   
	                                                 ) 

	{
	printf("\t migrate pid %d to *** ARM *** ret: %d ->\n", pid, ret);
 	ret = set_migration(pid, SIGUSR1);
	
	printf ("6 - ARM\n");
	}


/*CPU Load > ARMtsh ***  CPU Load > FPGAtsh ***  Kernel in XCLBIN *** Kernel Busy 	
           X	                 YES	               YES	            NO  --> FPGA */
if (                                   (cpu_work_load > fpga_tsh_load) &&  
	(kernel_in_xclbin == 1) &&  (kernel_busy == 0) ) 

	{
	if (arm_tsh_load > fpga_tsh_load)	
		{
		printf("\t migrate pid %d to *** FPGA *** ret: %d ->\n", pid, ret);
		ret = set_migration(pid, SIGUSR2);
		
		printf ("7A - FPGA\n");
		}
	
	else

		{
		printf("\t migrate pid %d to *** ARM *** ret: %d ->\n", pid, ret);
 		ret = set_migration(pid, SIGUSR1);
	
		printf ("7B - ARM\n");
		}
	
	}



// if (running_tsks % POPCORN_NUM_PLATFORMS == 1) {
	// if (running_tsks < 90) {

/*
if (cpu_work_load > fpga_tsh_load  ) {
		printf("\t migrate pid %d to *** X86 *** (stay)\n", pid);
		ret = set_migration(pid, SIGRTMIN);

	// } else if (running_tsks % POPCORN_NUM_PLATFORMS == 2) {
	// 	ret = set_migration(pid, SIGUSR1);
	// 	//printf("\t dbg - sent SIGUSR1 to pid %d ret: %d ->\n", pid, ret);
	// 	printf("\t migrate pid %d to *** ARM *** ret: %d ->\n", pid, ret);
	} else { /* 3 */
		//printf("\t dbg - sent SIGUSR2 to pid %d ret: %d ->\n", pid, ret);

//		printf("\t migrate pid %d to *** ARM *** ret: %d ->\n", pid, ret);
// 		ret = set_migration(pid, SIGUSR1);
	
/*
		printf("\t migrate pid %d to *** FPGA *** ret: %d ->\n", pid, ret);
		ret = set_migration(pid, SIGUSR2);
	}

*/



}

void sigchld_handler(int s)
{
#if 0
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
#else
	//printf("TODO - %s()\n", __func__);
#endif
}


/* net - get sockaddr, IPv4 or IPv6: */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



void time_handler2(size_t timer_id, void * user_data)
{


FILE *fp;
char str1[10], str2[10];
int proc_run, num_cpu;

// fp = popen("mpstat |grep all | awk {'print$13'}","r");
// fp = popen("top -n 1 | grep Cpu | awk {'print$8'} |sed 's/ni,//'","r");
//fp = popen("~/cpu-stat/bin/debug/cpu-stat","r");
//fp = popen("ps -e | grep pts | wc -l","r");

fp = popen("ps -r | wc -l","r");
fscanf(fp,"%s", str1);
proc_run = atoi(str1);
status = pclose(fp);


fp = popen("nproc","r");
fscanf(fp,"%s", str2);
status = pclose(fp);
num_cpu = atoi(str2);

//printf ("processes = %d ; CPUs = %d\n", proc_run, num_cpu);

//printf ("cpu flag=%d\n", cpu_work_load);

cpu_work_load = proc_run;

//printf ("cpu load =%d\n", cpu_work_load);


}


int main(int argc, char *argv[]) {
FILE *fp;
char cmd[100], cmd_str[50];
int i;

	struct sigaction sa;
  size_t timer2;

  initialize();


/* Not working  
// Enable XRT  
   strcpy(cmd,"source /opt/xilinx/xrt/setup.sh" );
   fp = popen(cmd,"r");
   fscanf(fp,"%s", cmd);
   status = pclose(fp);
*/
  
 
// Get number of HW kernels on the XCLBIN file
   strcpy(cmd,"xbutil query|grep -E '\\s*CU\\[\\s*.+\\]:.+:'|cut -f2 -d:|wc -l" );
// printf("CMD= %s\n", cmd);
   fp = popen(cmd,"r");
   fscanf(fp,"%s", cmd);
   kernel_qt =  atoi (cmd);
   status = pclose(fp);
// printf ("kernel_qt=%d\n", kernel_qt);



   for(i = 1; i <= kernel_qt; i++) {
   strcpy(cmd,"xbutil query|grep -E '\\s*CU\\[\\s*.+\\]:.+:'|cut -f2 -d:|head -" );
// printf("CMD1= %s\n", cmd);
   
// itoa(i,cmd_str,10);
   char result[10];	
   if (i==1) strcpy(result,"1");
   if (i==2) strcpy(result,"2");
   if (i==3) strcpy(result,"3");
   if (i==4) strcpy(result,"4");
   if (i==5) strcpy(result,"5");
   if (i==6) strcpy(result,"6");
   if (i==7) strcpy(result,"7");
   if (i==8) strcpy(result,"8");
   if (i==9) strcpy(result,"9");
   if (i==10) strcpy(result,"10");
   if (i==11) strcpy(result,"11");
   if (i==12) strcpy(result,"12");
	   

   strcat(cmd,result);
// printf("CMD2= %s\n", cmd);
   strcat(cmd,"|tail -1" );
// printf("CMD3= %s\n", cmd);
   fp = popen(cmd,"r");
   fscanf(fp,"%s", cmd);
   status = pclose(fp);
 
    strcpy(KNL_HW_Mod_XCLBIN[i],cmd);

    }

    printf ("******************** Xar-Trek Scheduler Server ****************\n");
    printf ("Available Hardware Kernels:\n");
    for(i = 1; i <= kernel_qt; i++) {

    	printf ("%s\n",KNL_HW_Mod_XCLBIN[i]);
	}
    printf ("\n"); 


	int rv, yes = 1;
	struct addrinfo hints, *servinfo, *p;
	int sockfd, new_fd, numbytes;  /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_storage their_addr; /* connector's address information */

	/* init */

	/* net */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(POPCORN_X86, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; /* reap all dead processes */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    puts("Popcorn_server: waiting for connections...\n");


  timer2 = start_timer(1, time_handler2, TIMER_PERIODIC, NULL);


	while(1) {
		char buf[MAXDATASIZE];
		char s[INET_ADDRSTRLEN];
        socklen_t sin_size = sizeof(their_addr);

		char *command, *process_id;
		const char *space = " ";

		/* Net */
		fflush(stdout);
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s);
        printf("server: got connection from %s\n", s);

        if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1)
            perror("send");
        close(new_fd);
        buf[numbytes] = '\0';

        printf("______________________________________________\n");
        printf("recv a raw message (%d): \"%s\"\n", numbytes, buf);

		/* Parse - 2 fields now */
		command = strtok(buf, space); /* App name or command */
		process_id = strtok(NULL,space); /* pid */
        if (!atoi(process_id)) {
            printf("wrong process id received %s\n", process_id);
            exit(1);
        }
		//printf("dbg - strtoked command %s\n", command);
		//printf("dbg - strtoked process_id %s\n", process_id);

		/* scheduler policy */
		printf("Now only triggered when an app send me a msg...\n");
		int pid = atoi(process_id);
		schedule(command, pid, buf);
		printf("\n\n");
	}
	/* Never end */

  stop_timer(timer2);

	// finalize();

	printf("Popcorn_server done. Hope you got good numbers.\n");
}
