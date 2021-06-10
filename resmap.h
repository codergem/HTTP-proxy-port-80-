//..............................................................................................................................

//ResEntry Class functions definition
#ifndef resmap_H
#define resmap_H
class ResInfo
{
    public:
    ResInfo(const char* uri, bool cache_enable);
};


class ResEntry
{
    public:
    char uri[max_uri_length];
    char method[10];
    char version[10];
    char hostname[max_hostname_length];
    char port[7];
    char content_length[7];
    unsigned int msg_size;
    char transfer_encoding_type[20];
    char complete_request[MAXLINE];
    char complete_response[MAXLINE];
    char ETAG[500];

    char vary_header[50];
    char vary_value[50];
    bool vary_to_be_checked;

    time_t last_modified_time_t;
    time_t expires_time_t;
    time_t date_of_cache_time_t;


    cache_node_list resp_cache; // just like the other variables in this class, even resp_cache gets memory allocated for it when calling mallocon ResEntry
    sem_t cache_access;
    ResEntry *next; // to maintain the linked list structure (hashing with chaining)

    ResEntry(char *uri);
    bool resource_is_stale();
    bool check_vary_presence(char *request);
    bool check_vary_match(char *request);
    unsigned int current_resource_age();
    unsigned int fresh_time_left();
    void update_timers();
    void update_resource_status();
    struct tm *get_current_tm_ptr();
    ~ResEntry();
};



//-------------------------------------------------
class Res_chain_list
{
    private :
    bool delete_success, uri_not_found;

    public :
    int known_hash_count;
    ResEntry *head,*tail;

    Res_chain_list();
    void insert(ResEntry *h);
    ResEntry* search(char *uri);
    bool remove(char *uri);
};


//-------------------------------------------------
/* ResMap Class Declaration */
class ResMap
{
    public:
    Res_chain_list *res_lists;

    ResMap();
    int HashFunc(const char *str);
    ResEntry* insert(char *uri); // return type is set so only for testing.
    ResEntry* search(char *uri); //this return type here is i think crucial for the code that is trying to get to the resinfoptr.
    bool remove(char *uri);
    ~ResMap();
};

#endif
