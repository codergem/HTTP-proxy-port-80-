#ifndef INADDR_NONE
#define INADDR_NONE     0xffffffff
#endif

#include "csapp.h"

int open_clientfd(const char *host, const char *service)
{
  return connectsock(host, service, "tcp");
}

// the below connectsock() function can be passed either tcp or udp transport protocol as argument
/*
 * Arguments in connectsock() :
 *      host      - name/address of host to which connection is desired
 *      service   - service associated with the desired port (could also be the port instead)
 *      transport - name of transport protocol to use ("tcp" or "udp")
 */

int connectsock(const char *host, const char *service, const char *transport )   // to allocate & connect a socket using TCP or UDP
{
        struct hostent  *host_name_ptr; // pointer to host information entry
        struct servent  *service_name_ptr;      // pointer to service information entry
        struct protoent *protocol_name_ptr;     // pointer to protocol information entry
        struct sockaddr_in sin; // an Internet endpoint address
        int    client_fd, type; // socket descriptor and socket type


        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;


//----------------------------------------------------------------------------------------------------
//finding the port number for the service mentioned (for ex, ssh : 22, DNS : 53)
        if ( service_name_ptr = getservbyname(service, transport) ) /// works if service name is given in command line
                sin.sin_port = service_name_ptr->s_port; //s_port is an integer
        else if ((sin.sin_port=htons((unsigned short)atoi(service))) == 0) /// works if service's port number (like DNS is 53) is given in command line
                err_sys("can't get the service entry\n");

//Using DNS to resolve the hostname
        if ( host_name_ptr = gethostbyname(host) ) // if the address given is a hostname
                memcpy(&sin.sin_addr, host_name_ptr->h_addr, host_name_ptr->h_length);
        else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE ) // checking if we have a specific address
                err_sys("can't get the host entry\n");

//getting the transport protocol's number (tcp : 6, udp : 17)
        if ( (protocol_name_ptr = getprotobyname(transport)) == 0)
                err_sys("can't get the protocol entry\n");

//determining socket type (stream or datagram)
        if (strcmp(transport, "udp") == 0)
                type = SOCK_DGRAM;  // if UDP
        else
                type = SOCK_STREAM; // if TCP
//-------------------------------------------------------------------------------------------



//Allocating the socket
        client_fd = socket(PF_INET, type, protocol_name_ptr->p_proto);
        if (client_fd < 0)
                err_sys("can't create socket");

//Calling connect on the socket
        if (connect(client_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
                err_sys("can't connect to the mentioned host and service");
        return client_fd;
}

