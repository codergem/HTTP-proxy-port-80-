#include "csapp.h"

char min_fresh_str[] = "min-fresh";
char max_age_str[] = "max-age";
char ETAG_str[] = "ETAG";

class ReqInfo
{
    public :
    unsigned int msg_size;
    int srv_fd;
    int max_age;
    int min_freshness_time; //if expires_time - current_time < min_freshness_time, then client does not want the cahced data.
    rio_t rio_server;
    rio_t rio_client;

    char method[10];
    char version[10];
    char uri[max_uri_length];
    char hostname[max_hostname_length];
    char port[10];
    char content_length[7];
    char transfer_encoding_type[20];
    char ETAG[500];
    char complete_request[MAXLINE];
    char response_headers[1000];
    char response_encoding_type[30];
    char temp_str[100];



    char filename[500];
    char cgiargs[500];

    cache_node_list resp_cache;

    ReqInfo()
    {
        msg_size = 0;
        method[0] = '\0';
        version[0] = '\0';
        uri[0] = '\0';
        hostname[0] = '\0';
        port[0] = '\0';
        content_length[0] = '\0';
        transfer_encoding_type[0] = '\0';
        ETAG[0] = '\0';
        complete_request[0] = '\0';

        filename[0] = '\0';
        cgiargs[0] = '\0';
    }
};


void cross_next_whitespace(char* final_ptr, char* curr_ptr, int max_length);
void copy_until_the_character(char* dst, char* src, char the_character, unsigned int max_length);
void copy_until_ptr(char* dst, char* ptr1, char* ptr2, int max_len);
char* findFirstOccurrence(const char *pattern, const char* text);
void computeFirstPrefix(char *str, int len, int* prefix, int patt_len);
int my_htoi(char *hex_string);
void copy_without_spaces(char *dst, char *src);


void serve_from_cache(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr);
void vary_based_serve(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr);
bool validate_on_etag(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr);
void serve_with_caching(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr);
void read_chunked_server_msg(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr);
void read_server_content(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr, unsigned int datalen);
void serve_without_caching(ReqInfo* req_info_ptr);
void tunnel_chunked_msg(ReqInfo* req_info_ptr);
void tunnel_content(ReqInfo* req_info_ptr, unsigned int datalen);
void tunnel_response(ReqInfo* req_info_ptr, unsigned int datalen);
void tunnel_chunked_response(ReqInfo* req_info_ptr);

char http_default_port[] = "80";
char head_host_name[] = "Host";
char head_user_agent[] = "User-Agent";
char head_accept[] = "Accept";
char head_accept_language[] = "Accept-Language";
char head_accept_encoding[] = "Accept-Encoding";
char head_cookie[] = "Cookie";
char head_connection[] = "Connection";
char head_upgrade_insecure_requests[] = "Upgrade-Insecure-Requests";
char head_content_length[] = "Content-Length";
char head_transfer_encoding[] = "Transfer-Encoding";
char value_chunked[] = "chunked";
char connection_close_header[] = "Connection: close\r\n";


extern HostMap host_map;

char crlf[] = "\r\n";
char colon[] = ":";



void mediator(int fd)//each thread runs this function
{
    char *temp1, buf[MAXLINE];
    ReqInfo req_info;
    ReqInfo* req_info_ptr = &req_info;
    ResEntry* res_entry_ptr = NULL;

    rio_readinitb(&(req_info_ptr->rio_client), fd);

    //step 1 : read client request
    //first, storing the complete request (before reading any message body) to a string in req_info.
    rio_readlineb(&(req_info_ptr->rio_client), buf, MAXLINE); //starting
    sscanf(buf, "%s %s %s", req_info_ptr->method, req_info_ptr->uri, req_info_ptr->version);

    int pos = strlen(req_info_ptr->version) - 1; // storing the request line. But, the '\r' at the end of line 1 may be the last byte of the version strimg before the null character of version. Handling that in these 3 lines.
    if(req_info_ptr->version[pos] == '\r')
        req_info_ptr->version[pos] = '\0';

    req_info_ptr->complete_request[0] = '\0'; //initializing
    while (strcasecmp(buf, crlf)) //strcmp returns zero only if buf has just "\r\n". Else, it returns some non-zero value that will execute the while loop.
    {
        strcat(req_info_ptr->complete_request, buf); //first execution stores the first request line in complete_request. buf[] still has the first request line.
        rio_readlineb(&(req_info_ptr->rio_client), buf, MAXLINE);
    }
    strcat(req_info_ptr->complete_request, connection_close_header);
    strcat(req_info_ptr->complete_request, buf);

    //parsing hostname
    header_value_parser(head_host_name, req_info_ptr->complete_request, req_info_ptr->hostname); //parameters are (pattern, src_string, dst_string). hostname will include the port number details too.

    //parsing port number if it is present in the request
    temp1 = findFirstOccurrence(colon, req_info_ptr->hostname);
    if (temp1 != NULL) //port number has been mentioned in the request
    {
       *temp1 = '\0';
        temp1++;
        int i = 0;
        while(temp1[i] != '\0')
        {
            req_info_ptr->port[i] = temp1[i];
            i++;
        }
        temp1[i] = '\0';
    }
    else
    {
        strcpy(req_info_ptr->port, http_default_port);
    }

    HostEntry *host_entry_ptr = host_map.search(req_info_ptr->hostname);
    if(host_entry_ptr != NULL) //if not null, we have an entry, so we can serve the request
    {
        if( !strcasecmp(req_info_ptr->method, "GET") ) //true indicates the method was a GET. We can check whether resource is cached.
        {
            if( host_entry_ptr->cache_enabled )
            {
                if( res_entry_ptr = host_entry_ptr->rmap.search(req_info_ptr->uri) ) //expression turns true when res_entry_ptr is not NULL
                {
                    if( findFirstOccurrence("no-store", req_info_ptr->complete_request) )
                    {
                        serve_without_caching(req_info_ptr);
                        if( res_entry_ptr->resource_is_stale() )
                        {
                            host_entry_ptr->rmap.remove(req_info_ptr->uri);
                        }
                        return;
                    }

                    else if( res_entry_ptr->resource_is_stale() )
                    {
                        serve_with_caching(req_info_ptr, res_entry_ptr);
                        return;
                    }

                    else if( findFirstOccurrence("no-cache", req_info_ptr->complete_request) )
                    {
                        if( temp1 = findFirstOccurrence(ETAG_str, req_info_ptr->complete_request) )
                        {
                            header_value_parser(ETAG_str, temp1, req_info_ptr->ETAG);
                            bool can_serve = validate_on_etag(req_info_ptr, res_entry_ptr);
                            if(can_serve)
                            {
                                vary_based_serve(req_info_ptr, res_entry_ptr);
                            }
                            return;
                        }
                    }

                    else if( temp1 = findFirstOccurrence(max_age_str, req_info_ptr->complete_request) )
                    {
                        header_value_parser(max_age_str, temp1, req_info_ptr->temp_str);
                        req_info_ptr->max_age = atoi(req_info_ptr->temp_str);
                        if (res_entry_ptr->current_resource_age() >= req_info_ptr->max_age)
                        {
                            serve_with_caching(req_info_ptr, res_entry_ptr);
                            return;
                        }
                        else
                        {
                            vary_based_serve(req_info_ptr, res_entry_ptr);
                            return;
                        }
                    }

                    else if( temp1 = findFirstOccurrence(min_fresh_str, req_info_ptr->complete_request) )
                    {
                        header_value_parser(min_fresh_str, temp1, req_info_ptr->temp_str);
                        req_info_ptr->min_freshness_time = atoi(req_info_ptr->temp_str);
                        if (res_entry_ptr->fresh_time_left() > req_info_ptr->min_freshness_time)
                        {
                            serve_with_caching(req_info_ptr, res_entry_ptr);
                            return;
                        }
                        else
                        {
                            vary_based_serve(req_info_ptr, res_entry_ptr);
                        }
                    }

                    else //no cache restrictions posed by client. We can serve from cache as our cache is also not stale (checked once before this).
                    {
                        vary_based_serve(req_info_ptr, res_entry_ptr);
                    }
                }

                else
                {
                    if( findFirstOccurrence("no-store", req_info_ptr->complete_request) )
                    {
                        serve_without_caching(req_info_ptr);
                        return;
                    }

                    else
                    {
                        res_entry_ptr = host_entry_ptr->rmap.insert(req_info_ptr->uri);
                        serve_with_caching(req_info_ptr, res_entry_ptr);
                        return;
                    }
                }
            }
            else
            {
                serve_without_caching(req_info_ptr);
                return;
            }
        }
        else //not a get method. We should just tunnel the communication and close the connection.
        {
            serve_without_caching(req_info_ptr);
        }
    }
    else
    {
        char rcode[] = "501";
        char rheading[] = "Hostname Not Served";
        char rmsg[] = "The proxy does not serve for this Hostname";
        clienterror(fd, req_info_ptr->method, rcode, rheading, rmsg);
    }

    return;
}


//---------------------------------------------------------------------------------------------------


void serve_from_cache(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr)
{
    //
}

void vary_based_serve(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr)
{
    if(res_entry_ptr->vary_to_be_checked)
    {
        if( res_entry_ptr->check_vary_presence(req_info_ptr->complete_request))
        {
            if( res_entry_ptr->check_vary_match(req_info_ptr->complete_request) )
                serve_from_cache(req_info_ptr, res_entry_ptr);
        }
        else
            serve_from_cache(req_info_ptr, res_entry_ptr);
    }
}



bool validate_on_etag(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr)
{
    int n;
    char buf[MAXLINE];
    char validation_request[1000];
    char* temp1;

    validation_request[0] = '\0';
    strcat(validation_request, "GET ");
    strcat(validation_request, req_info_ptr->uri);
    strcat(validation_request, " HTTP/1.1\r\n");
    strcat(validation_request, "If-None-Match: ");
    strcat(validation_request, req_info_ptr->ETAG);
    strcat(validation_request, "\r\n");
    strcat(validation_request, "Host: ");
    strcat(validation_request, req_info_ptr->hostname);
    strcat(validation_request, "\r\n\r\n");

    ////step 1 : open connection with server and tunnel client data if any to the server.
    req_info_ptr->srv_fd = open_clientfd(req_info_ptr->hostname, req_info_ptr->port);
    rio_readinitb( &(req_info_ptr->rio_server), req_info_ptr->srv_fd );

    //writing request headers to the server
    rio_writen( req_info_ptr->srv_fd, validation_request, strlen(validation_request));

    /////////////////////////////////reading server response

    n = rio_readlineb(&(req_info_ptr->rio_server), buf, MAXLINE); //reading first line
    if( findFirstOccurrence("304", buf) )
    {
        close(req_info_ptr->srv_fd);
        return true;
    }

    else if( findFirstOccurrence("200", buf) )
    {
        res_entry_ptr->complete_response[0] = '\0';
        while (strcasecmp(buf, crlf)) //expression says false if buf[] == "\r\n". Else, while loop executes again.
        {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            strcat(res_entry_ptr->complete_response, buf);
            n = rio_readlineb(&(req_info_ptr->rio_server), buf, MAXLINE);
        }
        rio_writen(req_info_ptr->rio_client.rio_fd, connection_close_header, strlen(connection_close_header));
        rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);

        //check if there is chunked encoding and if yes, read server's chunked message and forward it to the client.
        if( temp1 = findFirstOccurrence(head_transfer_encoding, res_entry_ptr->complete_response) )
        {
            header_value_parser(head_transfer_encoding, temp1, res_entry_ptr->transfer_encoding_type); //As temp1 is already at the beginning of the "Transfer-Encoding" string, using it as to reduce the amount of pattern matching required.
            if(strcmp(req_info_ptr->transfer_encoding_type, value_chunked) == 0) //if encoding type is "chunked", we tunnel the chunked message
            {
                read_chunked_server_msg(req_info_ptr, res_entry_ptr);
            }
        }

        //if client msg not chunked, checking if there is any msg at all and if yes, tunneling it to server.
        else if( temp1 = findFirstOccurrence(head_content_length, res_entry_ptr->complete_response) )
        {
            header_value_parser(head_content_length, temp1, req_info_ptr->content_length);
            req_info_ptr->msg_size = atoi(req_info_ptr->content_length);
            res_entry_ptr->msg_size = req_info_ptr->msg_size;
            read_server_content(req_info_ptr, res_entry_ptr, req_info_ptr->msg_size);
        }

        close(req_info_ptr->rio_server.rio_fd);
        res_entry_ptr->update_resource_status();
        return false;
    }

    else
    {
        printf("\n unexpected response code while etag validation. Received %s as first response line from server.", buf);
    }

}

void serve_with_caching(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr)
{
    strcpy(res_entry_ptr->complete_request, req_info_ptr->complete_request);
    strcpy(res_entry_ptr->hostname, req_info_ptr->hostname);
    strcpy(res_entry_ptr->content_length, req_info_ptr->content_length);
    strcpy(res_entry_ptr->port, req_info_ptr->port);
    strcpy(res_entry_ptr->transfer_encoding_type, req_info_ptr->transfer_encoding_type);
    strcpy(res_entry_ptr->ETAG, req_info_ptr->ETAG);
    strcpy(res_entry_ptr->method, req_info_ptr->method);

    int n;
    char buf[MAXLINE];
    char* temp1;

    ////step 1 : open connection with server and tunnel client data if any to the server.
    req_info_ptr->srv_fd = open_clientfd(req_info_ptr->hostname, req_info_ptr->port);
    rio_readinitb( &(req_info_ptr->rio_server), req_info_ptr->srv_fd );

    //writing request headers to the server
    rio_writen(req_info_ptr->srv_fd, req_info_ptr->complete_request, strlen(req_info_ptr->complete_request));

    //check if there is chunked encoding and if yes, read client's chunked message and forward it to the server.
    if( temp1 = findFirstOccurrence(head_transfer_encoding, req_info_ptr->complete_request) )
    {
        header_value_parser(head_transfer_encoding, temp1, req_info_ptr->transfer_encoding_type); //As temp1 is already at the beginning of the "Transfer-Encoding" string, using it as to reduce the amount of pattern matching required.
        if(strcmp(req_info_ptr->transfer_encoding_type, value_chunked) == 0) //if encoding type is "chunked", we tunnel the chunked message
        {
            tunnel_chunked_msg(req_info_ptr);
        }
    }

    //if client msg not chunked, checking if there is any msg at all and if yes, tunneling it to server.
    else if( temp1 = findFirstOccurrence(head_content_length, req_info_ptr->complete_request) )
    {
        header_value_parser(head_content_length, temp1, req_info_ptr->content_length);
        req_info_ptr->msg_size = atoi(req_info_ptr->content_length);
        tunnel_content(req_info_ptr, req_info_ptr->msg_size);
    }

    //Clearing the cache to store it with new data
    int cache_nodes_cleared = res_entry_ptr->resp_cache.reset();

    //Now, storing and forwarding the server's response to the client.
    //First, storing the response lines, before reading any data.
    bool connection_close_sent = false;
    n = rio_readlineb(&(req_info_ptr->rio_server), buf, MAXLINE);
    while (strcasecmp(buf, crlf)) //expression says false if buf[] == "\r\n". Else, while loop executes again.
    {
        if(n == 0) {
            printf("\nrio_readnb has returned a zero. This means the remote server closed the connection connection prematurely");
            close(req_info_ptr->rio_server.rio_fd); //closing connection because, by now we would have relayed all bytes received from the server to the client
            close(req_info_ptr->rio_client.rio_fd);
            return;
        }

        else if(n < 0 && errno != EINTR) {
            printf("\nSome error, because read() returned a negative value.");
            close(req_info_ptr->rio_server.rio_fd);
            close(req_info_ptr->rio_client.rio_fd);
            return;
        }

        else if (n > 0) {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            strcat(res_entry_ptr->complete_response, buf);
            if(findFirstOccurrence(connection_close_header, buf))
                connection_close_sent = true;
        }

        n = rio_readlineb(&(req_info_ptr->rio_server), buf, MAXLINE);
    }

    if(! connection_close_sent)
        rio_writen ( req_info_ptr->rio_client.rio_fd, connection_close_header, strlen(connection_close_header) );
    rio_writen (req_info_ptr->rio_client.rio_fd, buf, (size_t) n);   //in this call buf will always have "\r\n" (atlest expected to be so)


    //check if there is chunked encoding and if yes, read server's chunked message and forward it to the client.
    if( temp1 = findFirstOccurrence(head_transfer_encoding, res_entry_ptr->complete_response) )
    {
        header_value_parser(head_transfer_encoding, temp1, res_entry_ptr->transfer_encoding_type); //As temp1 is already at the beginning of the "Transfer-Encoding" string, using it as to reduce the amount of pattern matching required.
        if(strcmp(req_info_ptr->transfer_encoding_type, value_chunked) == 0) //if encoding type is "chunked", we tunnel the chunked message
        {
            read_chunked_server_msg(req_info_ptr, res_entry_ptr);
        }
    }

    //if client msg not chunked, checking if there is any msg at all and if yes, tunneling it to server.
    else if( temp1 = findFirstOccurrence(head_content_length, res_entry_ptr->complete_response) )
    {
        header_value_parser(head_content_length, temp1, req_info_ptr->content_length);
        req_info_ptr->msg_size = atoi(req_info_ptr->content_length);
        res_entry_ptr->msg_size = req_info_ptr->msg_size;
        read_server_content(req_info_ptr, res_entry_ptr, req_info_ptr->msg_size);
    }

    close(req_info_ptr->rio_server.rio_fd);
    close(req_info_ptr->rio_client.rio_fd);
    res_entry_ptr->update_resource_status();
}

void read_chunked_server_msg(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr)
{
    int buf_size = 1000, i=0;
    unsigned int chunk_size = 0;
    char buf[buf_size + 1], chunk_size_str[10];
    bool msg_ended = false;

    res_entry_ptr->msg_size = 0;

    rio_readlineb(&(req_info_ptr->rio_server), buf, buf_size);
    while(msg_ended == false)
    {
        while( (buf[i] >= 97 && buf[i] <= 102) || (buf[i] >= 65 && buf[i] <= 70) || (buf[i] >= 48 && buf[i] <= 57) ) //condition tests whether character is a valid hexadecimal digit
        {
            chunk_size_str[i] = buf[i];
            i++;
        }
        chunk_size_str[i] = '\0';
        chunk_size = my_htoi(chunk_size_str); //now we know the size of the chunk
        res_entry_ptr->msg_size += chunk_size;

        if(chunk_size == 0)
        {
            msg_ended = true;
            break;
        }
        else
        {
            read_server_content(req_info_ptr, res_entry_ptr, chunk_size);
        }
    }
}


void read_server_content(ReqInfo* req_info_ptr, ResEntry* res_entry_ptr, unsigned int datalen)
{
    int bytes_written = 0, bytes_to_be_written = datalen;
    int n, buf_size = 1000;
    char buf[buf_size + 1];

    if(bytes_to_be_written <= buf_size) //if message will completely fit in our string buffer
    {
        n = rio_readnb(&(req_info_ptr->rio_server), buf, bytes_to_be_written);
        if(n > 0)
        {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            size_t x = res_entry_ptr->resp_cache.cwrite(buf, n);
        }
        else if(n < 0)
        {
            printf("rio_readnb returned a negative value");
            return;
        }
    }

    else //if message won't completely fit in our string buffer
    {
        while(bytes_to_be_written >= buf_size)
        {
            n = rio_readnb((&req_info_ptr->rio_server), buf, buf_size);
            if(n > 0)
            {
                buf[n] == '\0';
                rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
                res_entry_ptr->resp_cache.cwrite(buf, n);
            }
            else if(n < 0)
            {
                printf("rio_readnb returned a negative value");
                break;
            }
            else if(n == 0)
            {
                printf("a client connection broke");
            }
            bytes_to_be_written -= n;
        }

        n = rio_readnb(&(req_info_ptr->rio_server), buf, bytes_to_be_written);
        if(n > 0)
        {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            size_t x = res_entry_ptr->resp_cache.cwrite(buf, n);
        }
        else if(n < 0)
        {
            printf("rio_readnb returned a negative value");
            return;
        }
    }
}


void serve_without_caching(ReqInfo* req_info_ptr)
{
    int n;
    char buf[MAXLINE];
    char* temp1;

    ////step 1 : open connection with server and tunnel client data if any to the server.
    req_info_ptr->srv_fd = open_clientfd(req_info_ptr->hostname, req_info_ptr->port);
    rio_readinitb( &(req_info_ptr->rio_server), req_info_ptr->srv_fd );

    //writing request headers to the server
    rio_writen(req_info_ptr->srv_fd, req_info_ptr->complete_request, strlen(req_info_ptr->complete_request));

    //check if there is chunked encoding and if yes, read client's chunked message and forward it to the server.
    if( temp1 = findFirstOccurrence(head_transfer_encoding, req_info_ptr->complete_request) )
    {
        header_value_parser(head_transfer_encoding, temp1, req_info_ptr->transfer_encoding_type); //As temp1 is already at the beginning of the "Transfer-Encoding" string, using it as to reduce the amount of pattern matching required.
        if(strcmp(req_info_ptr->transfer_encoding_type, value_chunked) == 0) //if encoding type is "chunked", we tunnel the chunked message
        {
            tunnel_chunked_msg(req_info_ptr);
        }
    }

    //if client msg not chunked, checking if there is any msg at all and if yes, tunneling it to server.
    else if( temp1 = findFirstOccurrence(head_content_length, req_info_ptr->complete_request) )
    {
        header_value_parser(head_content_length, temp1, req_info_ptr->content_length);
        req_info_ptr->msg_size = atoi(req_info_ptr->content_length);
        tunnel_content(req_info_ptr, req_info_ptr->msg_size);
    }

/*  //ignoring footers now. These lines for footers may never be needed.
    //now, forwarding any footers that we may receive from the client.
    n = rio_readlineb(&(req_info_ptr->rio_client), buf, MAXLINE);
    while (strcasecmp(buf, crlf))
    {
        rio_writen(req_info_ptr->rio_server.rio_fd, buf, (size_t) n);
        n = rio_readlineb(&(req_info_ptr->rio_client), buf, MAXLINE);
    }
    rio_writen(req_info_ptr->rio_server.rio_fd, buf, (size_t) n);
    //by now, the client's complete request (including any message), has been sent to server.
*/


    //Now, forwarding the server's response to the client.
    bool connection_close_sent = false;
    req_info_ptr->response_headers[0] = '\0';
    n = rio_readlineb(&(req_info_ptr->rio_server), buf, MAXLINE);
    while (strcasecmp(buf, crlf)) //expression says false if buf[] == "\r\n". Else, while loop executes again.
    {
        if(n == 0) {
            printf("\nrio_readnb has returned a zero. This means the remote server closed the connection connection prematurely");
            close(req_info_ptr->rio_server.rio_fd); //closing connection because, by now we would have relayed all bytes received from the server to the client
            close(req_info_ptr->rio_client.rio_fd);
            return;
        }

        else if(n < 0 && errno != EINTR) {
            printf("\nSome error, because read() returned a negative value.");
            close(req_info_ptr->rio_server.rio_fd);
            close(req_info_ptr->rio_client.rio_fd);
            return;
        }

        else if (n > 0) {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            strcat(req_info_ptr->response_headers, buf);
            if(findFirstOccurrence(connection_close_header, buf))
                connection_close_sent = true;
        }

        n = rio_readlineb(&(req_info_ptr->rio_server), buf, MAXLINE);
    }


    if(! connection_close_sent)
        rio_writen ( req_info_ptr->rio_client.rio_fd, connection_close_header, strlen(connection_close_header) );
    rio_writen (req_info_ptr->rio_client.rio_fd, buf, (size_t) n);   //in this call buf will always have "\r\n" (atlest expected to be so)


    if( temp1 = findFirstOccurrence(head_transfer_encoding, req_info_ptr->response_headers) )
    {
        header_value_parser(head_transfer_encoding, temp1, req_info_ptr->response_encoding_type); //As temp1 is already at the beginning of the "Transfer-Encoding" string, using it as to reduce the amount of pattern matching required.
        if(strcmp(req_info_ptr->response_encoding_type, value_chunked) == 0) //if encoding type is "chunked", we tunnel the chunked message
        {
            tunnel_chunked_response(req_info_ptr);
        }
    }

    else if( temp1 = findFirstOccurrence(head_content_length, req_info_ptr->response_headers) )
    {
        while (1)
        {
            n = rio_readnb( &(req_info_ptr->rio_server), buf, MAXLINE);
            if(n == 0)  break; // EOF
            else if (n > 0)  rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            else if(n < 0 && errno != EINTR)
            {
                printf("\nSome error, because read() returned a negative value");
                break;
            }
        }
    }


    close(req_info_ptr->rio_server.rio_fd);
    close(req_info_ptr->rio_client.rio_fd);
}

void tunnel_chunked_msg(ReqInfo* req_info_ptr) //assumed that the next data to read from cli_fd is the first byte after the last crlf that ends the request or response headers.
{
    int buf_size = 1000;
    char buf[buf_size + 1], chunk_size_str[10]; //the programmer has to allocate 'str_size + 1' bytes of memory for strings. The plus one byte is to store the null character.
    unsigned int chunk_size = 0; //will have the chunk size after parsing the chunk_size_str[] string.
    bool msg_ended = false;

    rio_readlineb(&(req_info_ptr->rio_client), buf, buf_size); //expected to read the size of first chunk, but if we read another crlf before the chunk, we are handling that too.
    int i = 0;
    while( ( buf[i] == '\r' && buf[i + 1] == '\n' ) || (buf[i] == ' ') || (buf[i] == '\n') || (buf[i] == '\t') || (buf[i] == '\r')) //crossing unexpected linefeeds or carriage returns or whitespaces.
    {
        i++;
    }

    while(msg_ended == false)
    {
        while( (buf[i] >= 97 && buf[i] <= 102) || (buf[i] >= 65 && buf[i] <= 70) || (buf[i] >= 48 && buf[i] <= 57) ) //condition tests whether character is a valid hexadecimal digit
        {
            chunk_size_str[i] = buf[i];
            i++;
        }
        chunk_size_str[i] = '\0';
        chunk_size = my_htoi(chunk_size_str); //now we know the size of the chunk

        if(chunk_size == 0)
        {
            msg_ended = true;
            break;
        }
        else
        {
            tunnel_content(req_info_ptr, chunk_size);
        }
    }
}




int cross_empty_line(char *buf, int i)
{
    while( ( buf[i] == '\r' && buf[i + 1] == '\n' ) || (buf[i] == ' ') || (buf[i] == '\n') || (buf[i] == '\t') || (buf[i] == '\r')) //crossing unexpected linefeeds or carriage returns or whitespaces.
        i++;

    return i;
}

void tunnel_chunked_response(ReqInfo* req_info_ptr) //assumed that the next data to read from cli_fd is the first byte after the last crlf that ends the request or response headers.
{
    int buf_size = 1000, i, j, n;
    char buf[buf_size + 1], chunk_size_str[10]; //the programmer has to allocate 'str_size + 1' bytes of memory for strings. The plus one byte is to store the null character.
    unsigned int chunk_size = 0; //will have the chunk size after parsing the chunk_size_str[] string.
    bool msg_ended = false;

    while(1)
    {
        i = 0;
        j = 0;
        n = rio_readlineb(&(req_info_ptr->rio_server), buf, buf_size); //expected to read the size of first chunk, but if we read another crlf before the chunk, we are handling that too.
        i = cross_empty_line(buf, i);
        if(i == n) 
        {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) i);
            continue; //happens when its an empty line with buf pointing at "\r\n"
        }

        if(msg_ended == false)
        {
            if(n == 0) 
            {
                msg_ended == true;
                break;
            }
            while( (buf[i] >= 97 && buf[i] <= 102) || (buf[i] >= 65 && buf[i] <= 70) || (buf[i] >= 48 && buf[i] <= 57) ) //condition tests whether character is a valid hexadecimal digit
            {
                chunk_size_str[i] = buf[j];
                i++;
                j++;
            }
            chunk_size_str[j] = '\0';
            chunk_size = my_htoi(chunk_size_str); //now we know the size of the chunk

            if(chunk_size == 0)
            {
                rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
                msg_ended = true;
                break;
            }
            else
            {
                rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
                tunnel_response(req_info_ptr, chunk_size);
            }
        }

        if(msg_ended == true) break;
    }
}


void tunnel_content(ReqInfo* req_info_ptr, unsigned int datalen)
{
    int bytes_written = 0, bytes_to_be_written = datalen;
    int n, buf_size = 1000;
    char buf[buf_size + 1]; //the programmer has to allocate 'str_size + 1' bytes of memory for strings. The plus one byte is to store the null character.

    if(bytes_to_be_written <= buf_size) //if message will completely fit in our string buffer
    {
        n = rio_readnb(&(req_info_ptr->rio_client), buf, bytes_to_be_written); //rio_readnb() doesn't add a null character to the end of the bytes
        if(n > 0)
            rio_writen(req_info_ptr->rio_server.rio_fd, buf, (size_t) n);
        else if(n == 0)
        {
            printf("\na client connection broke");
        }
        else if(n < 0 && errno != EINTR)
        {
            printf("\nSome error, because read() returned a negative value");
        }
    }

    else //when message won't completely fit in our string buffer
    {
        while(bytes_to_be_written >= buf_size)
        {
            n = rio_readnb(&(req_info_ptr->rio_client), buf, buf_size);
            if(n > 0)
            {
                buf[n] == '\0';
                rio_writen(req_info_ptr->rio_server.rio_fd, buf, (size_t) n);
            }
            else if(n == 0)
            {
                printf("\na client connection broke");
            }
            else if(n < 0 && errno != EINTR)
            {
                printf("\nSome error, because read() returned a negative value");
                break;
            }
            bytes_to_be_written -= n;
        }

        n = rio_readnb(&(req_info_ptr->rio_client), buf, bytes_to_be_written);
        if(n > 0)
        {
            rio_writen(req_info_ptr->rio_server.rio_fd, buf, (size_t) n);
        }
        else if(n < 0)
        {
            printf("\nrio_readnb returned a negative value");
            return;
        }
    }
}


void tunnel_response(ReqInfo* req_info_ptr, unsigned int datalen)
{
    int bytes_written = 0, bytes_to_be_written = datalen;
    int n, buf_size = 1000;
    char buf[buf_size + 1]; //the programmer has to allocate 'str_size + 1' bytes of memory for strings. The plus one byte is to store the null character.

    if(bytes_to_be_written <= buf_size) //if message will completely fit in our string buffer
    {
        n = rio_readnb(&(req_info_ptr->rio_server), buf, bytes_to_be_written); //rio_readnb() doesn't add a null character to the end of the bytes
        if(n > 0)
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
        else if(n == 0)
        {
            printf("\na client connection broke");
        }
        else if(n < 0 && errno != EINTR)
        {
            printf("\nSome error, because read() returned a negative value");
        }
    }

    else //when message won't completely fit in our string buffer
    {
        while(bytes_to_be_written >= buf_size)
        {
            n = rio_readnb(&(req_info_ptr->rio_server), buf, buf_size);
            if(n > 0)
            {
                buf[n] == '\0';
                rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
            }
            else if(n == 0)
            {
                printf("\na client connection broke");
            }
            else if(n < 0 && errno != EINTR)
            {
                printf("\nSome error, because read() returned a negative value");
                break;
            }
            bytes_to_be_written -= n;
        }

        //now we have less than buf_size bytes to be read
        n = rio_readnb(&(req_info_ptr->rio_server), buf, bytes_to_be_written);
        if(n > 0)
        {
            rio_writen(req_info_ptr->rio_client.rio_fd, buf, (size_t) n);
        }
        else if(n < 0)
        {
            printf("\nrio_readnb returned a negative value");
            return;
        }
    }
}



//---------------------------------------------------------------------------------------------------


int my_htoi(char *hex_string)
{
    unsigned int num_of_digits = strlen(hex_string);
    int i = num_of_digits, multiplier = 1;
    int n, sum=0;
    char c;

    while(i > 0)
    {
        c = hex_string[i-1];

        if( c >= 97 && c <= 102 )
            n = c - 87;
        else if( c >= 65 && c <= 70 )
            n = c - 55;
        else if( c >= 48 && c <= 57 )
            n = c - 48;

        sum += n*multiplier;
        multiplier *= 16;
        i--;
    }

    return sum;
}

void copy_until_ptr(char* dst, char* ptr1, char* ptr2, int max_len)
{
    int i=0;
    while( (i < max_len-1) && (&ptr1[i] != ptr2) )
    {
        dst[i] = ptr1[i];
        i++;
    }
    dst[i] = '\0';
}

void copy_until_the_character(char* dst, char* src, char the_character, unsigned int max_length)
{
    int i=0;
    while( (i < max_length-1)  &&  (src[i] != the_character) )
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}


void copy_without_spaces(char *dst, char *src)
{
    char* temp1 = src;
    while(*temp1 == ' ' || *temp1 == '\t')
    {
        temp1++;
    }

    size_t len = 0;
    char* temp2 = temp1;
    while (*temp2 != '\0')
    {
        temp2++;
        len++;
    }

    temp2--;
    dst[len] = '\0';
    while(*temp2 == ' ' || *temp2 == '\t')
    {
        temp2--;
        len--;
    }

    while(len != 0)
    {
        *dst = *temp1;
        dst++;
        temp1++;
        len--;
    }
}


int header_value_parser(char* head_name, const char *buf, char* head_value, char separating_character) //ex : (head_host_name, buf, req_info_ptr->hostname). It stores the value for a header in the dst string
{
    char *cptr = findFirstOccurrence(head_name, buf); //parameters are (pattern, text). temp1 will point to H in Host.
    cptr += strlen(head_name);

    while(*cptr == ' ' || *cptr == '\t' || *cptr == separating_character)
    {
        cptr++;
    }

    int i = 0;
    while( ! (cptr[i] == '\r' && cptr[i+1] == '\n') )
    {
        head_value[i] = cptr[i]; //start position of the value for the particular header
        i++;
    }
    head_value[i] = '\0';

    return i; //btw, this function has stored the value for the header in the dst[] string. Also it has added a null to the end of dst string.
}

char* findFirstOccurrence(const char *pattern, const char* text)//finds the first occurrence of a string (the pattern argument) in the text and returns a pointer to it.
{
    int patt_len = strlen(pattern);
    int text_len = strlen(text);

//    char *whole = new char(patt_len + text_len + 1 + 1); // extra 1 byte for the dollar character and an extra 1 byte for the null character.
    char *whole = (char*) malloc((patt_len + text_len + 1 + 1)*sizeof(char)); // extra 1 byte for the dollar character and an extra 1 byte for the null character.

    strcpy(whole, pattern);
    whole[patt_len] = '$';
    strcpy(&whole[patt_len] + 1, text);

    int whole_len = patt_len + text_len + 1;
    int *prefix = (int*) malloc(sizeof(int)*whole_len);

    computeFirstPrefix(whole, whole_len, prefix, patt_len); // prefix array now has our max_border values for each index in the whole string.

    // now let's find the position where we have our first occurrence of the pattern
    int i = patt_len + 1;
    while(i < whole_len)
    {
        if(prefix[i] != patt_len)
            i++;
        else
            break;
    }

    if(i >= whole_len)
        return NULL;

    else
    {
        char *first_match_ptr = whole;
        first_match_ptr = first_match_ptr + i - patt_len + 1;
        return first_match_ptr;
    }
}

void computeFirstPrefix(char *str, int len, int* prefix, int patt_len) //used by findFirstOccurrence()
{
    prefix[0] = 0;
    int border = 0;

    for(int i=1 ; i < len ; i++)
    {
        while(border > 0 && toupper(str[i]) != toupper(str[border]))
        {
            border = prefix[border - 1];
            if(border == patt_len)
                border = prefix[border -1];
        }


        if (toupper(str[i]) == toupper(str[border]))
            border += 1;
        else
            border = 0;


        prefix[i] = border;

        //the below if statement is present only because, in the http proxy, we bother only about the first occurrence of our header in the request/response. Though this function is capable of finding further occurrences, we don't need them for now. So, we break the loop as soon as we find first occurrence and do not populate the prefix array completely.
        if(prefix[i] == patt_len)
        {
            break;
        }
    }
}

void computeWholePrefix(char *str, int len, int* prefix, int patt_len) //used by findFirstOccurrence()
{
    prefix[0] = 0;
    int border = 0;

    for(int i=1 ; i < len ; i++)
    {
        while(border > 0 && toupper(str[i]) != toupper(str[border]))
        {
            border = prefix[border - 1];
            if(border == patt_len)
                border = prefix[border -1];
        }


        if (toupper(str[i]) == toupper(str[border]))
            border += 1;
        else
            border = 0;


        prefix[i] = border;

    }
}

char* findAllOccurrences(const char *pattern, const char* text, int *prefix)//finds the first occurrence of a string (the pattern argument) in the text and returns a pointer to it.
{
    int patt_len = strlen(pattern);
    int text_len = strlen(text);

//    char *whole = new char(patt_len + text_len + 1 + 1); // extra 1 byte for the dollar character and an extra 1 byte for the null character.
    char *whole = (char*) malloc((patt_len + text_len + 1 + 1)*sizeof(char)); // extra 1 byte for the dollar character and an extra 1 byte for the null character.

    strcpy(whole, pattern);
    whole[patt_len] = '$';
    strcpy(&whole[patt_len] + 1, text);

    int whole_len = patt_len + text_len + 1;
    //int *prefix = (int*) malloc(sizeof(int)*whole_len);

    computeWholePrefix(whole, whole_len, prefix, patt_len); // prefix array now has our max_border values for each index in the whole string.

    // now let's find the position where we have our first occurrence of the pattern
    int i = patt_len + 1;
    while(i < whole_len)
    {
        if(prefix[i] != patt_len)
            i++;
        else
            break;
    }

    if(i >= whole_len)
        return NULL;

    else
    {
        char *first_match_ptr = whole;
        first_match_ptr = first_match_ptr + i - patt_len + 1;
        return first_match_ptr;
    }
}


//---------------------------------------------------------------------------------------------------

void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin"))   // Static content
    {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html");
        return 1;
    }

    else  /* Dynamic content */
    {

        ptr = strchr(uri, '?'); //strchr returns a pointer to the first occurrence of '?' in the uri string.

        if (ptr) // works if ptr is not a NULL pointer
        {
            strcpy(cgiargs, ptr+1); //characters starting from ptr+1 until a '\0' character is found. '\0' is also included in the copy.
            *ptr = '\0'; //we are adding another '/0' anyway.
        }

        else
        {
            strcpy(cgiargs, "");
        }

        strcpy(filename, ".");
        strcat(filename, uri); //so, the program and its lcation is defined by ./uri. The cgiags string is input to the program.
        return 0;
    }
}
