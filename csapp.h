#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <cstdlib>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include "content_cacher.h"

#define MAXLINE 65535
#define MAXBUF   8192
#define max_hostname_length 200
#define max_regex_length 1000
#define max_uri_length 3000
#define HASH_TABLE_SIZE 128
#define RIO_BUFSIZE 8192


typedef struct sockaddr SA;
typedef void sigfunc(int);
sigfunc *signal (int signo, sigfunc *func);

int sem_init(sem_t *sem, int, unsigned int value);
int sem_wait(sem_t *s); /* wrapper will be P(s) */
int sem_post(sem_t *s); /* wrapper will be V(s) */

typedef struct {  //this structure is used in RIO.c
    int rio_fd;  //file descriptor
    int rio_cnt; //number of unread bytes in this structure's internal buffer
    char *rio_bufptr; //pointer to next unread byte in this structure's internal buffer
    char rio_buf[RIO_BUFSIZE]; //the structure's internal buffer. We have a large buffer size in this structure to read from the file in advance before the user perform's the read.
} rio_t;


void rio_readinitb(rio_t *rp, int fd);
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);


int err_sys(const char *err_msg);
int errexit(const char *format, ...);
int connectsock(const char *host, const char *service, const char *transport );
int connectUDP(const char *host, const char *service);
int open_clientfd(const char *host, const char *service);
int open_listenfd(int port, char *dott_dec_addr = NULL);


void mediator(int fd);
void doit(int fd);
int parse_uri(char *uri, char *filename, char *cgiargs);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg);
void cross_next_whitespace(char* final_ptr, char* curr_ptr, int max_length);
void copy_until_the_character(char* dst, char* src, char the_character, unsigned int max_length);
void copy_until_ptr(char* dst, char* ptr1, char* ptr2, int max_len);
void copy_without_spaces(char *dst, char *src);
char* findFirstOccurrence(const char *pattern, const char* text);
void computeFirstPrefix(char *str, int len, int* prefix, int patt_len);
int header_value_parser(char* head_name, const char *buf, char* head_value, char separating_character = ':');
int my_htoi(char *hex_string);


//..............................................................................................................................

//..............................................................................................................................

