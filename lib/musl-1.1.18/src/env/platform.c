#include <errno.h>
#include "syscall.h"
#include "platform.h"
#include "arch.h"

/* Moved to libmigration to accomodate glibc.  */

#if defined __aarch64__ || defined __powerpc64__ || defined __riscv64__ || defined __x86_64__

int popcorn_getnid() __attribute__((weak));
int popcorn_getthreadinfo(struct popcorn_thread_status *a) __attribute__((weak));
int popcorn_getnodeinfo(int *a, struct popcorn_node_status *b) __attribute__((weak));

int popcorn_getnid_musl() {
  struct popcorn_thread_status status;
  if (syscall(SYS_get_thread_status, &status)) return -1;
  return status.current_nid;
}

int popcorn_getthreadinfo_musl(struct popcorn_thread_status *status) {
  return syscall(SYS_get_thread_status, status);
}

int popcorn_getnodeinfo_musl(int *origin,
                        struct popcorn_node_status status[MAX_POPCORN_NODES]) {
  int ret, i;

  if (!origin || !status) {
    errno = EINVAL;
    return -1;
  }

  ret = syscall(SYS_get_node_info, origin, status);
  if (ret) {
    for (i = 0; i < MAX_POPCORN_NODES; i++) {
      status[i].status = 0;
      status[i].arch = ARCH_UNKNOWN;
      status[i].distance = -1;
    }
    *origin = -1;
  }
  return ret;
}

#else

#define RETURN_ERROR \
  do { \
    errno = ENOSYS; \
    return -1; \
  } while(0);

int popcorn_getnid_musl() { RETURN_ERROR }
int popcorn_getthreadinfo_musl(struct popcorn_thread_status *a) { RETURN_ERROR }
int popcorn_getnodeinfo_musl(int *a, struct popcorn_node_status *b) { RETURN_ERROR }

#endif

