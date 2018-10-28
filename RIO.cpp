#include"csapp.h"

//rio_readn() and rio_writen() are similar to unix's read() and write() except for relieving the user programmer from having to worry about the short counts returned when performing read or write. These are unbuffered read and write functions.

//What are buffered read and write ? If we are to search for a specific character in a file and we do this by performing a read, we will have to check each byte that we read. And, we will need the kernel's help in reading each byte and passing them over to our user memory and therefore this method requires a trap to the kernel for each time a byte is read (i.e. we have to switch between kernel, user and memory cycles for each byte to be read and checked). The solution to this problem is that we read bytes in advance from our files/sockets before the user program performs the read. So, when user performs the read, this is done from the user memory (because it was read and kept ready in user program's memory) and we perform our character check in this user memory. So, kernel need not be invoked each time unless the internal user buffer is full and needs to be filled again (which happens once in a long while).

/*
#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;  //file descriptor
    int rio_cnt; //number of unread bytes in this structure's internal buffer
    char *rio_bufptr; //pointer to next unread byte in this structure's internal buffer
    char rio_buf[RIO_BUFSIZE]; //the structure's internal buffer. We have a large buffer size in this structure to read from the file in advance before the user perform's the read.
} rio_t;
*/

void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}


//very important function because it is used by rio_readlineb.
//checkout the memcpy()
//this function can only read a maximum of sizeof(rp->rio_buf) at once.
//This function returns zero only when read() on the socketreturns zero. If read() returns zero, it means the other end has sent a FIN.
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) //
{
    int cnt;

    while (rp->rio_cnt <= 0)  /* to refill if buf is empty */ // rio_cnt=0 means there are no more bytes to read in the buffer
    {
        int bytes_available = 0;
        ioctl(rp->rio_fd, FIONREAD, &bytes_available);

        if (bytes_available < 0)
        {
            if (errno != EINTR) /* Interrupted by sig handler return */
                return -1;
        }
        else if (bytes_available == 0) bytes_available = 1; //we don't want to give the read call zero bytes to read as the third parameter
        else if (bytes_available > sizeof(rp->rio_buf))
            bytes_available = sizeof(rp->rio_buf);

        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, bytes_available); //this is the line where all available bytes are pre-fetched into the rio_buf string. This is where the idea of the RIO package is implemented. Its the line that serves the purpose.

        if (rp->rio_cnt < 0)
        {
            if (errno != EINTR) /* Interrupted by sig handler return */
                return -1;
        }
        else if (rp->rio_cnt == 0) /* EOF */ //Randal Bryant says : An application understands that EOF has been reached when read() returns zero.
            return 0;

        else
            rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rp->rio_cnt < n)
        cnt = rp->rio_cnt;

    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}


// rio_readlineb : Has a counter n that states the total no of characters read. Reads until a '\n' is found. (n-1)th byte will be the '\n' (i.e. usrbuf[n-2] = '\n'). The nth byte will be a '\0' that is added by the function (i.e the '\0' is not from the socket, but artificially added). If n-1 characters were read before a '\n' was found, then a '\0' is added as the nth character and the function exits.

// So, the caller may want to check if there is a '\n' or not in the usrbuf[n-2] postition before deciding whether the complete line was read.

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, read_count;
    char c, *bufp = (char *) usrbuf;
    for (n = 1; n < maxlen; n++)  //here we start from n=1 because we only want to read n-1 characters from the socket. The nth character will be a null that we will add to usrbuf before exiting the function.
    {
        if ((read_count = rio_read(rp, &c, 1)) == 1) //though you are reading only one byte here, rio_read() does prefetch of RIO_BUFSIZE no of bytes from the socket/fd at the backend.
        {
            *bufp++ = c;
            if (c == '\n')  //this is how we sense the line to be ending. Also, please note that we read the \n also into the buffer.
                break;
        }

        else if (read_count == 0)
        {
            if (n == 1)
                return 0; // EOF before any data read
            else
                break;    // EOF after some data was read
        }

        else
            return -1;        /* Error */
    }

    *bufp = 0; //please note that we are adding a null character after the '\n'.
    return n; //n is number of bytes read excluding the null charater.
}


ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) //please note that readnb() does not add a null character to the end of the string of characters that have been read into the buffer.
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = (char *) usrbuf;

    while (nleft > 0)
    {
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
        {
            if (errno == EINTR) /* Interrupted by sig handler return */
                nread = 0;     /* Call read() again */
            else
                return -1;     /* errno set by read() */
        }

        else if (nread == 0)
            break;            /*because we have reached EOF */

        nleft -= nread;
        bufp += nread;
    }

    return (n - nleft);   /* Return >= 0 */
}


ssize_t rio_readn(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = (char *) usrbuf;

    while (nleft > 0) {

        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR)    /* Interrupted by sig handler return */
                nread = 0;             /* and call read() again */
            else
                return -1;             /* errno set by read() */
        }

        else if (nread == 0)
            break;
        /* EOF */
        nleft -= nread;
        bufp += nread;
    }

    return (n - nleft); /* Return >= 0 */
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = (char *) usrbuf;

    while (nleft > 0) {

        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)     /* Interrupted by sig handler return */
                nwritten = 0;           /* and call write() again */
            else
                return -1;              /* errno set by write() */
        }

        nleft -= nwritten;
        bufp += nwritten;
    }

    return n;
}

