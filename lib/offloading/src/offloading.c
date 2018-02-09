#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include "migrate.h"
#include <sys/types.h>
#include <limits.h>


pid_t tid;
int node;
int core;
int origin_node;
#define SYS_POPCORN_PROPOSE_MIGRATE 331

int
request_migrate(int nid)
{
        return syscall(SYS_POPCORN_PROPOSE_MIGRATE, tid, nid);
}

void offloading_callback(void* data)
{
  cpu_set_t set;

  /* cancel old callback */
  register_migrate_callback(NULL, NULL);

  /* set core */
  CPU_ZERO(&set);
  CPU_SET(core, &set);
  if (sched_setaffinity(tid, sizeof(set), &set) == -1)
     perror("sched_setaffinity");
}
void offloading_destroy(void);
int main(int argc, char **argv , char **envp);

/* Read environment variables to setup migration points. */
//void __attribute__((constructor))
//void
int
//offloading_init(void)
offloading_init(int argc, char **argv , char **envp)
{
  const char *node_str;
  const char *core_str;

  tid=getpid();
  node_str = getenv("POPCORN_DESTINATION_NODE");
  core_str = getenv("POPCORN_DESTINATION_CORE");

  printf("%s: destination node %s, destination core %s\n", __func__, node_str, core_str);

  if(node_str != NULL && core_str != NULL)
  {

  node = strtol(node_str, NULL, 10);
  core = strtol(core_str, NULL, 10);
  origin_node = current_nid();
  

  register_migrate_callback(offloading_callback, NULL);
  request_migrate(node);
  }

  int ret = main(argc, argv, envp);

  if(node_str != NULL && core_str != NULL)
    offloading_destroy();

  return ret;
}

void exit_at_origin(void)
{
  migrate(get_origin_nid(), NULL, NULL);
}

//void __attribute__((destructor))
void
offloading_destroy(void)
{
  exit_at_origin();
  printf("%s: destination node %d, destination core %d\n", __func__, node, core);
}
