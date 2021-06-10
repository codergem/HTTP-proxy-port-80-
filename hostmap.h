//..............................................................................................................................
#ifndef hostmap_H
#define hostmap_H

// HostEntry Class Declaration
class HostEntry
{
    public:
    char hostname[max_hostname_length];
    char regex_for_cache[max_regex_length];
    bool cache_enabled;
    HostEntry *next; // the nodes are to form a linked list.
    ResMap rmap;

    //contructor
    HostEntry(char *hostname, bool cache_enable);
};

//-------------------------------------------------
class Host_chain_list
{
    private :
    bool delete_success, hostname_not_found;

    public :
    int *my_edges, known_hash_count;
    HostEntry *head,*tail;

    Host_chain_list();
    void insert(HostEntry *h);
    HostEntry* search(char *hostname);
    bool remove(char *hostname);
};


//-------------------------------------------------

/* HostMap Class Declaration */
class HostMap
{
    public:

    Host_chain_list *host_chain_lists;
    int no_of_hostnames;

    int HashFunc(const char *str);
    bool remove(char *hostname);
    HostEntry* insert(char *hostname, bool cache_enable = false); // return type is set so only for testing.
    HostEntry* search(char *hostname); //this return type here is i think crucial for the code that is trying to get to the hostinfoptr.

    HostMap();
    ~HostMap();
};

#endif
