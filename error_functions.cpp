#include"csapp.h"

int err_sys(const char *err_msg)
{
    fprintf(stderr, "%s\n",err_msg);
    puts(strerror(errno));
    exit(-1);
}

int errexit(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  exit(1);
}
