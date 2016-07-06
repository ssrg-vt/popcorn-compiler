// Copyright (c) Antonio Barbalace, SSRG, VT 2014
// NOTE most of these info can be found using getrusage(..) API

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int dump_status_self (void);
int dump_sched_self (void);
int parse_file(const char * file, char * list[], int entries);
static void parse_line(char * src, char * list[], int entries);

#undef TESTING
#ifdef TESTING
int main(int argc, char* argv[])
{
  dump_status_self();
  dump_sched_self();
  
  return 0;
}
#endif

#define FILE1 "/proc/self/status"
#define STRINGA1_1 "voluntary_ctxt_switches:"
#define STRINGA1_2 "nonvoluntary_ctxt_switches:"

int dump_status_self (void)
{
  char file[] = FILE1;
  char * list[] = {STRINGA1_1, STRINGA1_2};
  int entries =2;
 
  return parse_file(file, list, entries);
}

#define FILE2 "/proc/self/sched"
#define STRINGA2_1 "se.sum_exec_runtime"
#define STRINGA2_2 "se.statistics.wait_sum"
#define STRINGA2_3 "se.statistics.iowait_sum"
#define STRINGA2_4 "se.nr_migrations"
#define STRINGA2_5 "nr_switches"
#define STRINGA2_6 "nr_voluntary_switches"
#define STRINGA2_7 "nr_involuntary_switches"

int dump_sched_self (void)
{
  char file[] = FILE2;
  char * list[] = {STRINGA2_1, STRINGA2_2, STRINGA2_3, STRINGA2_4, STRINGA2_5, STRINGA2_6, STRINGA2_7};
  int entries =7;
  
  return parse_file(file, list, entries);
}

#define MAXBUFFER 128
static char buffer[MAXBUFFER];

// Parse file for content  
int parse_file(const char * file, char * list[], int entries)
{
  int fd =-1;
  long offset =0;
  char * cstart =0, * cend =0;
  ssize_t sret;
  off_t soff, noff;
  
  fd=open(file, O_RDONLY);
  if (fd == -1) {
    perror(file);
    return -1;
  }
  
  memset(buffer, 0, MAXBUFFER);
  while ( (sret = read(fd, buffer, (MAXBUFFER -1))) != 0 ) {
    cstart =buffer;
    while ( (cend = strchr(cstart, '\n')) != 0) {
      *cend = 0;
      //printf("LINE: %s\n", cstart);
      parse_line(cstart, list, entries);
      cstart = ++cend;
    }
    soff = -((unsigned long)(buffer + sret) - (unsigned long)cstart);
    noff=lseek(fd, soff, SEEK_CUR);
    if (noff == -1) {
      perror("lseek");
      close(fd);
      return -1;
    }
  }

  close(fd);
  return 0;
}

static void parse_line(char * src, char * list[], int entries)
{
  char* cstart =src;
  int len, i;

  for (i=0; i<entries; i++) { 
    len = strlen(list[i]);
    if ( strncmp(src, list[i], len) == 0 ) {
      cstart += len;
      goto _found;
    }
  }
  return;

_found:
  //printf("LINE: %s\n", src);
  printf("%s\n", src);

#undef DO_PARSE  
#ifdef DO_PARSE
  while (*cstart == ' ' || *cstart == '\t' || *cstart == ':')
    cstart++;
  printf("%s\n", cstart);
#endif  
}
