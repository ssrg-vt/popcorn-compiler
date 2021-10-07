/*
 * Popcorn scheduler client for FPGA
 * Copyright (C) 2020 Ho-Ren (Jack) Chuang <horenc@vt.edu>
 *
 * Distributed under terms of the MIT license.
 */


#include <time.h>
#include <sys/time.h>


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

// Fox 3 (NODE0) and Fox 7 (NODE1) machines 
//#define POPCORN_NODE0 "10.4.4.33" /* TODO - change it according to your setup */
//#define POPCORN_NODE1 "10.4.4.37" /* TODO - change it according to your setup */

// AISB Machines: DELL 7920 (NODE0) and LEG (NODE1)
#define POPCORN_NODE0 "10.1.1.45" /* TODO - change it according to your setup */
#define POPCORN_NODE1 "10.1.1.51" /* TODO - change it according to your setup */

// AISB Machines: DELL 7920 (NODE0) and MIR2 (NODE1)
//#define POPCORN_NODE0 "10.1.1.45" /* TODO - change it according to your setup */
//#define POPCORN_NODE1 "10.1.10.12" /* TODO - change it according to your setup */





#define PORT "3490"  /* the port users will be connecting to */
#define BACKLOG 128  /* how many pending connections (applications) queue will hold */
#define MAXDATASIZE 128 /* msg size */

int per_app_migration_flag = -1;

/***
 * Compile: gcc -o popcorn_sched_client_test popcorn_sched_client_test.c
 */


 void do_work0(int sig_id)
 {
 	if (!per_app_migration_flag)
 		per_app_migration_flag = 0;
 	else if (per_app_migration_flag == -1)
 		per_app_migration_flag = 0;
 	else // ==1
 		per_app_migration_flag = 0;
 	printf("\t ->%s(): got signal from Popcorn server sig_id %d set flag to %d\n",
 										__func__, sig_id, per_app_migration_flag);
 }

 void do_work1(int sig_id)
 {
 	if (!per_app_migration_flag)
 		per_app_migration_flag = 1;
 	else if (per_app_migration_flag == -1)
 		per_app_migration_flag = 1;
 	else // ==1
 		per_app_migration_flag = 0;
 	printf("\t ->%s(): got signal from Popcorn server sig_id %d set flag to %d\n",
 										__func__, sig_id, per_app_migration_flag);
 }

void do_work2(int sig_id)
{
	if (!per_app_migration_flag)
		per_app_migration_flag = 2;
	else if (per_app_migration_flag == -1)
		per_app_migration_flag = 2;
	else // ==2
		per_app_migration_flag = 0;
	printf("\t ->%s(): got signal from Popcorn server sig_id %d set flag to %d\n",
										__func__, sig_id, per_app_migration_flag);
}


/***
 * type - 0: start 1: end
 * Put this function at the begining/end of your main so that
 * your application can talk w/ Popcorn scheduer.
 *
 * e.g.,
 *	void main() {
 *		popcorn_client(0);
 *		(your app code ....)
 *		popcorn_client(1);
 *  }
 *
 */
struct timeval starts, ends;
//long double start_sec, start_usec, end_sec, end_usec;
int cpu_load;
char cpu_load_str[10];
extern const char *__progname;
char str1[100], cmd[100], THR_Line[10];
FILE *fp, *fp2;

int popcorn_client(int type)
{
//	FILE *fp, *fp2;
//	char str1[100], cmd[100], THR_Line[10];
	int  x86_exec, FPGA_EXE, ARM_exec;
	char x86_exec_str[100];
	int  FPGA_THR, ARM_THR, status;
	long long mig_exec;

	int sockfd, rv;
	struct addrinfo hints, *servinfo, *p;
	//int numbytes;
	//char s[INET6_ADDRSTRLEN];

  /* Register signal handlers when starting a process */
  if (!type) {
    printf("My ppid %d pid %d\n", getppid(), getpid());
    signal(SIGRTMIN, do_work0);
    signal(SIGUSR1, do_work1);
    signal(SIGUSR2, do_work2);
  }

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(POPCORN_NODE0, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
				perror("client: socket");
				continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				perror("client: connect");
				close(sockfd);
				continue;
		}

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure
	char out_going[MAXDATASIZE];
	switch(type){
		/*	case 0: I'm at the beginning of the program
			case 1: I'm at the end of the program
			case TODO: I'm before migration
			case TODO: I'm after migration */
		case 0:
		{
			snprintf(out_going, MAXDATASIZE, "%s %d",
					 //__progname + (strlen(__progname)-1),
					 __progname,
					 getpid());


  			gettimeofday(&starts, 0);
			
			// Gets CPU Load before executing the app
			fp = popen("ps -r| wc -l","r");
			fscanf(fp,"%s", cpu_load_str);
			status = pclose(fp);
			cpu_load = atoi(cpu_load_str);
  		

		break;
		}
		case 1:
		{
		snprintf(out_going, MAXDATASIZE, "%s %d","END",getpid());


		// Get actual executing time of migrated function
		gettimeofday(&ends, 0);
  		mig_exec = (ends.tv_sec - starts.tv_sec) * 1000000LL + ends.tv_usec - starts.tv_usec;
		printf("SCH---> Finish PID = %d (%s); Exec Time = %lld us; CPU_LOAD = %i\n", getpid(), __progname, mig_exec,cpu_load);
	


		switch (per_app_migration_flag) {

			// Target = x86 (did not migrate)
			case 0:
			{	
                        // Read FPGA recorded exec time
			strcpy(cmd,"grep -E '\\s*" );
   			strcat(cmd,__progname );
   			strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt | cut -f3 -d,| cut -f2 -d,");
   			fp = popen(cmd,"r");
			fscanf(fp,"%s", str1);
                        status = pclose(fp);
                        FPGA_EXE = atoi(str1);

			// Read ARM recorded exec time
   			strcpy(cmd,"grep -E '\\s*" );
   			strcat(cmd,__progname );
   			strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt | cut -f4 -d,| cut -f2 -d,");
   			fp = popen(cmd,"r");
			fscanf(fp,"%s", str1);
   			status = pclose(fp);
                        ARM_exec = atoi(str1);

			// Read FPGA Threshold
        		//grep -E '\s*FaceDet320\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f3 -d,| cut -f2 -d,
                	strcpy(cmd,"grep -E '\\s*" );
                        strcat(cmd,__progname );
                        strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f3 -d,| cut -f2 -d,");
                        //printf("CMD= %s\n", cmd);
			fp = popen(cmd,"r");
                        fscanf(fp,"%s", str1);
                        status = pclose(fp);
                        FPGA_THR = atoi(str1);
	
			// Read ARM Threshold
        		//grep -E '\s*FaceDet320\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f4 -d,| cut -f2 -d,
                	strcpy(cmd,"grep -E '\\s*" );
                        strcat(cmd,__progname );
                        strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f4 -d,| cut -f2 -d,");
                        //printf("CMD= %s\n", cmd);
			fp = popen(cmd,"r");
                        fscanf(fp,"%s", str1);
                        status = pclose(fp);
                        ARM_THR = atoi(str1);
			
			printf("SCH---> Target = x86; FPGA (EXE = %i; THR = %i; ARM (EXE = %i; THR = %i)\n", FPGA_EXE, FPGA_THR, ARM_exec, ARM_THR);
	

			if ((mig_exec > FPGA_EXE) && (cpu_load < FPGA_THR) ) {
			   printf ("SCH---> Update Threshold FPGA = %i\n", cpu_load);
			   // Get line number with the FPGA threshold	
			   // grep -n -E <program_name>  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| cut -f1 -d:
			   strcpy(cmd,"grep -n -E '\\s*" );
			   strcat(cmd,__progname );
                           strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f1 -d:");
                           fp = popen(cmd,"r");
                           fscanf(fp,"%s", THR_Line);
                           status = pclose(fp);
			   //printf ("FPGA threshold line = %s\n",THR_Line);
			   
			   // Update the FPGA threshold	   
			   // The inner grep+awk search for the FPGA threshold and replace it
			   // The sed replace the entire line containing the FPGA threshold
			   // sed -i <TRHESHOLD_LINE>s/.*/`grep -E '\s*<Prog_Name>\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| 
			   // awk -F, -v OFS=, '{$3="<CPU_LOAD>"; print }'`/g ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt
			   strcpy(cmd,"sed -i " );
			   strcat(cmd, THR_Line );
                           strcat(cmd, "s/.*/");
			   strcat(cmd,"`grep -E '\\s*");
			   strcat(cmd,__progname);
			   strcat(cmd,"\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| awk -F, -v OFS=, '{$3=\"");
			   strcat(cmd,cpu_load_str);
			   strcat(cmd,"\"; print }'`/g  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt");
			   fp = popen(cmd,"r");
                           status = pclose(fp);
			   	
			}
			else
			{
			   if ((mig_exec > ARM_exec) && (cpu_load < ARM_THR))  {
			   printf ("SCH---> Update Threshold ARM = %i\n", cpu_load);
			   // Get line number with the ARM threshold	
			   // grep -n -E <program_name>  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| cut -f1 -d:
			   strcpy(cmd,"grep -n -E '\\s*" );
			   strcat(cmd,__progname );
                           strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f1 -d:");
                           fp = popen(cmd,"r");
                           fscanf(fp,"%s", THR_Line);
                           status = pclose(fp);
			   //printf ("FPGA threshold line = %s\n",THR_Line);
			   
			   // Update the ARM threshold	   
			   // The inner grep+awk search for the ARM threshold and replace it
			   // The sed replace the entire line containing the ARM threshold
			   // sed -i <TRHESHOLD_LINE>s/.*/`grep -E '\s*<Prog_Name>\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| 
			   // awk -F, -v OFS=, '{$4="<CPU_LOAD>"; print }'`/g ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt
			   strcpy(cmd,"sed -i " );
			   strcat(cmd, THR_Line );
                           strcat(cmd, "s/.*/");
			   strcat(cmd,"`grep -E '\\s*");
			   strcat(cmd,__progname);
			   strcat(cmd,"\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| awk -F, -v OFS=, '{$4=\"");
			   strcat(cmd,cpu_load_str);
			   strcat(cmd,"\"; print }'`/g  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt");
			   fp = popen(cmd,"r");
                           status = pclose(fp);
					
			
			
			
			
			
			   }
			   else {
			   
			   sprintf(x86_exec_str,"%d",mig_exec);
			   printf ("SCH---> Update EXE x86 = %s\n", x86_exec_str);
//			   x86_exec = mig_exec;	
		           // Get line number with the x86 Exec Time	
			   // grep -n -E <program_name>  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt| cut -f1 -d:
			   strcpy(cmd,"grep -n -E '\\s*" );
			   strcat(cmd,__progname );
                           strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt | cut -f1 -d:");
                           fp = popen(cmd,"r");
                           fscanf(fp,"%s", str1);
                           status = pclose(fp);
			   //printf ("FPGA threshold line = %s\n",THR_Line);
			   
			   // Update the x86 Exec Time   
			   // The inner grep+awk search for the x86 Exec Time and replace it
			   // The sed replace the entire line containing the x86 Exec Time
			   // sed -i <TRHESHOLD_LINE>s/.*/`grep -E '\s*<Prog_Name>\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt| 
			   // awk -F, -v OFS=, '{$4="<CPU_LOAD>"; print }'`/g ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt
			   strcpy(cmd,"sed -i " );
			   strcat(cmd, str1 );
                           strcat(cmd, "s/.*/");
			   strcat(cmd,"`grep -E '\\s*");
			   strcat(cmd,__progname);
			   strcat(cmd,"\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt| awk -F, -v OFS=, '{$2=\"");
			   strcat(cmd,x86_exec_str);
			   strcat(cmd,"\"; print }'`/g  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt");
			   fp = popen(cmd,"r");
                           status = pclose(fp);
					
			
				

			   }

			
			
			}
			
			
			break;
			}	

			// Target = ARM
			case 1:
			{
                        printf("SCH---> Target = ARM; ");
			FILE *fp_exec=NULL;			
			FILE *fp_sched=NULL;		
			FILE *fp_sched2=NULL;
    			char *tok1, *tok2, *tok3, *tok4;
    			char full_line[100], new_line[100];
    			int ARM_THR, count;
    			char str[100];
    			int line;	

                        // Read x86 recorded exec time
			
			// Open File
    			while (!fp_exec)
    			{
       			  fp_exec = fopen("/home/edshor/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt", "r");
		          //printf("fp_exec\n");
    			}


    			line=1;
    			while ((fgets(str, 1000, fp_exec)) != NULL)
    			{
        		//printf("Line(%i) = %s\n", line, str);
        		strcpy (full_line, str);
        		if (strstr(str, __progname))
        		{
                	  tok1 = strtok(str,",");
                	  tok2 = strtok(NULL,",");
                	  tok3 = strtok(NULL,",");
                	  tok4 = strtok(NULL,",");
                	  //printf ("Found = %s\n", tok);
                	  //fprintf (fp_exec, "%s", str);
                	  break;
        		}
    			line++;

    			}

    			// Close file
    			fclose (fp_exec);

			x86_exec = atoi(tok2);
			printf ("x86 Exec = %i ", x86_exec);
	
			
			// If ARM exec time greater than x86 exec time 
			// Increase ARM Threshold
			if (mig_exec > x86_exec) {
   			
				// Read ARM Threshold
                        	// Open File
                        	while (!fp_sched)
                        	{
                          	  fp_sched = fopen("/home/edshor/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt", "r");
                        	  //printf("fp_sched\n");
                        	}


                        	line=1;
                        	while ((fgets(str, 1000, fp_exec)) != NULL)
                        	{
                        	  //printf("Line(%i) = %s\n", line, str);
                        	  strcpy (full_line, str);
                          	  if (strstr(str, __progname))
                        	    {
                          	    tok1 = strtok(str,",");
                          	    tok2 = strtok(NULL,",");
                          	    tok3 = strtok(NULL,",");
                          	    tok4 = strtok(NULL,",");
                          	    //printf ("Found = %s\n", tok);
                          	    //fprintf (fp_exec, "%s", str);
                          	    break;
                        	    }
                        	line++;

                        	}

                        	// Close file
                        	fclose (fp_sched);

				// Increase ARM Threshold
                        	ARM_THR = atoi(tok4);
				ARM_THR = ARM_THR + 1;
				sprintf (tok4, "%d", ARM_THR);
                        	printf ("; INC Threshold ARM = %i\n", ARM_THR);
	
			
				// Update scheduler threshold file	
				FILE *fp_sched2=NULL;
    				// Open File
    				while (!fp_sched2)
    				{
       				  fp_sched2 = fopen("/home/edshor/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt", "r+");
       				  //printf("fp_sched2\n");
    				}


				// Update line with new threshold
	    			strcpy (new_line, tok1);
    				strcat (new_line,",");
    				strcat (new_line, tok2);
				strcat (new_line,",");
    				strcat (new_line, tok3);
    				strcat (new_line,",");
    				strcat (new_line, tok4);




    				// Replace line in file
   				count = 0;
   				//printf ("Count = %i\n", count);
   				while ((fgets(str, 1000, fp_sched2)) != NULL)
    				{
        			  count++;
        			  //printf ("count = %i line = %i (%s) \n", count, line, str);

        			// If current line is line to replace/
        			if (count == line - 1) {
				  fputs(new_line, fp_sched2);
            			  //printf ("New Line  = %s\n", new_line);
            			break;
        			}
    				}

    				// Close file
    				fclose(fp_sched2);

			
			
			
			}			   
			
			else printf ("\n");

			break;
			}	

			// Target = FPGA
			case 2:
			{	
			printf("SCH---> Target = FPGA; ");


			// Read x86 recorded exec time
                        strcpy(cmd,"grep -E '\\s*" );
                        strcat(cmd,__progname );
                        strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Exec.txt | cut -f2 -d,| cut -f2 -d,");
                        fp = popen(cmd,"r");
                        fscanf(fp,"%s", str1);
                        status = pclose(fp);
                        x86_exec = atoi(str1);
			printf("x86 Exec = %i ", x86_exec);
			





			if (mig_exec > x86_exec) {
			   // Read and increase FPGA Threshold
                           strcpy(cmd,"grep -E '\\s*" );
                           strcat(cmd,__progname );
                           strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f3 -d,| cut -f2 -d,");
                           fp = popen(cmd,"r");
                           fscanf(fp,"%s", str1);
                           status = pclose(fp);
                           FPGA_THR = atoi(str1) + 1;
			   printf("; INC Threshold FPGA = %i\n", FPGA_THR);
			   sprintf(cpu_load_str,"%d",FPGA_THR);
			   //printf("STR1=%s\n",str1);	

			   // Get line number with the FPGA threshold	
			   // grep -n -E <program_name>  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| cut -f1 -d:
			   strcpy(cmd,"grep -n -E '\\s*" );
			   strcat(cmd,__progname );
                           strcat(cmd, "\\s*,.+,'  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt | cut -f1 -d:");
                           fp = popen(cmd,"r");
                           fscanf(fp,"%s", THR_Line);
                           status = pclose(fp);
			   //printf ("FPGA threshold line = %s\n",THR_Line);
			   
			   // Update the FPGA threshold	   
			   // The inner grep+awk search for the FPGA threshold and replace it
			   // The sed replace the entire line containing the FPGA threshold
			   // sed -i <TRHESHOLD_LINE>s/.*/`grep -E '\s*<Prog_Name>\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| 
			   // awk -F, -v OFS=, '{$3="<CPU_LOAD>"; print }'`/g ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt
			   strcpy(cmd,"sed -i " );
			   strcat(cmd, THR_Line );
                           strcat(cmd, "s/.*/");
			   strcat(cmd,"`grep -E '\\s*");
			   strcat(cmd,__progname);
			   strcat(cmd,"\\s*,.+,' ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt| awk -F, -v OFS=, '{$3=\"");
			   strcat(cmd, cpu_load_str );
			   strcat(cmd,"\"; print }'`/g  ~/Pop_Scheduler/popcorn-scheduler/KNL_HW_Sched.txt");
			   fp = popen(cmd,"r");
                           status = pclose(fp);




			
		
			}
                        
			else printf ("\n");

			break;
			}	




			default:
                	{
                        	printf( "Error on migration Flag\n");
                        
                	}

			}// switch (per_app_migration_flag)	
		
			
		break;
		}
		default:
		{
			fprintf(stderr, "server received wrong type %d \n", type);
			return 2;
		}
	} //switch (type)
	printf("\tdbg - out_going \"%s\" ->\n", out_going);
	if(send(sockfd,out_going,strlen(out_going), 0) == -1) {
        perror("send");
	}
  printf("\tdbg - out_going \"%s\" SUCCEED ->\n", out_going);
	close(sockfd);


	return 0;
}
