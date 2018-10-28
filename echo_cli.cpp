#include "csapp.h"

char request[] = "GET / HTTP/1.1\r\nHost: www.blue.com\r\n\r\n";
char response[MAXLINE];
char buf[MAXLINE];

int main(int argc, char **argv)
{
    int clientfd;
    char *port;
    char *host;
    char crlf[] = "\r\n";
    rio_t rio_client;


    if (argc != 3) {
        fprintf(stderr, "\nusage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];
    clientfd = open_clientfd(host, port);
    rio_readinitb(&rio_client, clientfd);

    rio_writen(clientfd, request, strlen(request));
    fputs(buf, stdout);

    int n = rio_readlineb(&rio_client, buf, MAXLINE);
    while (strcasecmp(buf, crlf)) //expression says false if buf[] == "\r\n". Else, while loop executes again.
    {
        if(n == 0) {
            printf("\nrio_readnb has returned a zero. This means the remote peer closed the connection connection prematurely");
            break;
        }

        else if(n < 0 && errno != EINTR) {
            printf("\nSome error, because read() returned a negative value.");
            break;
        }

        else if (n > 0) strcat(response, buf);
        n = rio_readlineb(&rio_client, buf, MAXLINE);
    }
    if(n > 0) strcat(response, buf);

    while (1)
    {
        n = rio_readnb( &(rio_client), buf, MAXLINE);
        if(n == 0)  break; // EOF
        else if (n > 0)  strcat(response, buf);
        else if(errno != EINTR) {
            printf("\nSome error, because read() returned a negative value");
            break;
        }
    }

    printf("\nresponse was :\n%s", response);

/* //below is the actual code for echo client. But, we are testing the http proxy now. So, currently using the 3 lines of code above.
    while (fgets(buf, MAXLINE, stdin) != NULL) {
        rio_writen(clientfd, buf, strlen(buf));
        rio_readlineb(&rio_client, buf, MAXLINE);
        fputs(buf, stdout);
    }
*/
    printf("\n");
    close(clientfd);
}
