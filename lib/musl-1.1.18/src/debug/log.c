#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#define BUF_SIZE 32

int popcorn_log(const char *format, ...)
{
  int ret = -1;
  char buf[BUF_SIZE];
  FILE *fp;
  va_list ap;

  snprintf(buf, BUF_SIZE, "/tmp/%d.log", gettid());
  fp = fopen(buf, "a");
  if(fp) {
    va_start(ap, format);
    ret = vfprintf(fp, format, ap);
    fclose(fp);
  }
  return ret;
}

