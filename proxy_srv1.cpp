#include "csapp.h"

HostMap host_map;

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

void echo(int connfd);
void *thread(void *vargp);
void *read_web_master_input(void *vargp);

int connfd_pipe[2];
sem_t connfd_mutex;

int main(int argc, char **argv)
{
    int listenfd, *connfdp, port;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;


    //void sig_chld(int); //not using processes. Only threads. So, we can ignore this for now.
    signal(SIGPIPE, SIG_IGN);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <address> <port>\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[2]);
    listenfd = open_listenfd(port,argv[1]);
    if(listenfd < 0)
    {
        printf("\ncould not set up a listener, open_listenfd() returned value < -1\n");
        exit(0);
    }
    else if (listenfd > 1)
    {
        printf("\n we may have a valid listenfd");
    }

    if (pipe(connfd_pipe) < 0)  //client serving threads get their connfd from this.
    {
        printf("\npipe error: [%s]\n",strerror(errno));
                err_sys("pipe error");
    }
    sem_init(&connfd_mutex, 0, 1);

//  code below is for debugging
//    pthread_create(&tid, NULL, read_web_master_input, NULL);
    char host1[] = "www.aryaka.com";
    HostEntry* host_entry_ptr = host_map.insert(host1); //adding new hostname to the hash table (or hash map if you will).
    host_entry_ptr->cache_enabled = false;
    printf("  hostname \"%s\" added with cache_flag = false", host_entry_ptr->hostname);

    char host2[] = "www.mi.com";
    host_entry_ptr = host_map.insert(host2); //adding new hostname to the hash table (or hash map if you will).
    host_entry_ptr->cache_enabled = false;


    char host3[] = "www.blue.com";
    host_entry_ptr = host_map.insert(host3); //adding new hostname to the hash table (or hash map if you will).
    host_entry_ptr->cache_enabled = false;
//debug code done


    connfdp = (int *) malloc(sizeof(int));

    int dummy_ctr = 0;

    while (1)
    {
        *connfdp = accept(listenfd, (SA *) &clientaddr, &clientlen);
        if(*connfdp < 1)
            continue;
        write(connfd_pipe[1], connfdp, sizeof(int));
        pthread_create(&tid, NULL, thread, NULL);
        dummy_ctr++;
    }
}



void *thread(void *vargp)
{
    int connfd;
    sem_wait(&connfd_mutex);
    read(connfd_pipe[0], &connfd, sizeof(int));
    sem_post(&connfd_mutex);
    pthread_detach(pthread_self());
    mediator(connfd); //depending on mediator to close the connfd for now.
    //close(connfd);
    return NULL;
}


void *read_web_master_input(void *vargp)
{
    int client_option;
    char hostname[max_hostname_length];
    char *uri;
    HostEntry *host_entry_ptr;

    while(1)
    {
        printf("\n\nOptions for Web master :");
        printf("\n 1. add a hostname to cache for");
        printf("\n 2. check whether a hostname is being served");
        printf("\n 3. disable cache for a hostname");
        printf("\n 4. enable cache for a hostname");
        printf("\n 5. check whether a uri is cached");
        printf("\n 6. list all resources cached for a hostname");
        printf("\n 7. list all hostnames setup");
        printf("\n 8. list the total number of hostnames configured");
        printf("\n 9. delete a hostname from the config");
        printf("\n\n");

        scanf("%d", &client_option);

        switch(client_option)
        {
            case 1 :  //add a hostname to cache for
            {
                printf("\n  enter new hostname below here : ");
                scanf("%s", hostname);  //first read new hostname from user. We will add this to our hash table for hostnames.
                if ( (host_entry_ptr = host_map.search(hostname)) != NULL )
                {
                    printf("\n  hostname \"%s\"already being served", host_entry_ptr->hostname);
                    break;
                }

                HostEntry* host_entry_ptr = host_map.insert(hostname); //adding new hostname to the hash table (or hash map if you will).
                printf("  hostname \"%s\" added with cache_flag = true (default)", host_entry_ptr->hostname);
                //The insert() call actually returns a pointer a host_entry object which is the actual entry in the hash table. But, we don't need it here for now (unless you want to test the hash table implementation).
                break;
            }

            case 2 : //check whether a hostname is being served
            {
                printf("\n  enter hostname to be checked : ");
                scanf("%s", hostname);
                host_entry_ptr = host_map.search(hostname);
                if(host_entry_ptr != NULL)
                    printf("  hostname present in hash table ");
                else
                    printf("  hostname not in hash table ");
                break;
            }

            case 3 : //disable cache for a hostname
            {
                printf("\n  enter hostname to disable cache for here : ");
                scanf("%s", hostname);
                break;
            }

            case 4 : //enable cache for a hostname
            {
                printf("\n  enter hostname to enable cache for here : ");
                scanf("%s", hostname);
                break;
            }

            case 5 : //check whether a uri is cached
            {
                printf("\n  enter hostname for the resource : ");
                scanf("%s", hostname);
                printf("\n  enter resource uri : ");
                scanf("%s", uri);
                break;
            }

            case 6 : //list all resources cached for a hostname
            {
                printf("\n  enter hostname to list resources for : ");
                scanf("%s", hostname);
                break;
            }

            case 8 : //list the current number of hostnames setup
            {
                printf("current number of hostnames configured := %d\n", host_map.no_of_hostnames);
                break;
            }

            case 9 : //delete a hostname from the config
            {
                bool removed = false;
                printf("\n  enter hostname to be deleted from config : ");
                scanf("%s", hostname);
                removed = host_map.remove(hostname);
                if(removed)
                    printf("  removed \"%s\" successfully", hostname);
                break;
            }

            default :
                printf("\n  enter a valid option number ");
                break;
        }
    }
}

//test function to verify thread presence in server setup
void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];

    rio_t rio;
    rio_readinitb(&rio, connfd);

    while((n = rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %d bytes\n", (int) n);
        rio_writen(connfd, buf, n);
    }
}

