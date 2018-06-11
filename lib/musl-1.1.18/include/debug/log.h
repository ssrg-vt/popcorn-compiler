#ifndef _DEBUG_LOG_H
#define _DEBUG_LOG_H

#include <unistd.h>

#define BUF_SIZE 32

/* Open the specified file in preparation for logging messages. */
#define popcorn_log_open_file( fn ) FILE *__popcorn_fp = fopen(fn, "a");

/* Open a generic per-thread file in preparation for logging messages. */
#define popcorn_log_open() \
  char __popcorn_buf[BUF_SIZE]; \
  snprintf(__popcorn_buf, BUF_SIZE, "/tmp/%d.log", gettid()); \
  popcorn_log_open_file(__popcorn_buf)

/* Write a message to the log. */
// Note: must call popcorn_log_open() before popcorn_log()!
#define popcorn_log( msg, ... ) \
  if(__popcorn_fp) fprintf(__popcorn_fp, msg, ##__VA_ARGS__);

/* Close the log. */
// Note: must call popcorn_log_open() before popcorn_log_close()!
#define popcorn_log_close() if(__popcorn_fp) fclose(__popcorn_fp);

/*
 * Open a per-thread log, write the specified message and close the log.  Valid
 * on all architectures without any file descriptor operations.
 */
#define popcorn_log_single( msg, ... ) \
  do { \
    char buf[BUF_SIZE]; \
    snprintf(buf, BUF_SIZE, "/tmp/%d.log", gettid()); \
    FILE *fp = fopen(buf, "a"); \
    if(fp) { \
      fprintf(fp, msg, ##__VA_ARGS__); \
      fclose(fp); \
    } \
  } while(0);

#endif

