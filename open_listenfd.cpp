#include"csapp.h"
#define LISTENQ 1024

using namespace std;


//helper function that combines socket, bind and listen functions. It returns a listening decriptor.
int open_listenfd(int port, char *dott_dec_addr)
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
    struct in_addr inp; //if address to bind is mentioned in the dott_dec_addr argument, this 'inp' variable is used to derive the network address from the dotted decimal value.

    //Creating a socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

    /* the next 2 code lines eliminate "Address already in use" error when we later call bind */
    //the server is being terminated and restarted immediately.
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
    return -1;

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;

    uint32_t server_network_address = htonl(INADDR_ANY);
    //'if' body below to derive network address from dotted decimal representation of the IPv4 address.
    if(dott_dec_addr != NULL){
        inet_aton(dott_dec_addr, &inp);
        server_network_address = inp.s_addr;
    }
    serveraddr.sin_addr.s_addr = server_network_address;

    serveraddr.sin_port = htons((unsigned short)port); //calling bind now that we have the information to do that
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
    return -1;

    //calling listen to start accepting client requests.
    if (listen(listenfd, LISTENQ) < 0)
    return -1;
    return listenfd;
}

