#include "csapp.h"

//HostEntry Class functions definition
HostEntry::HostEntry(char *hostname, bool cache_enable)
{
    strcpy(this->hostname, hostname);
    this->cache_enabled = cache_enable;
    this->next = NULL;
}

//Host_chain_list is a linked list. The hash table is an array of linked lists. Array actually contains pointers to linked lists.
Host_chain_list::Host_chain_list()
{
    this->delete_success = true;
    this->hostname_not_found = false;
    this->head = NULL;
    this->tail = NULL;
}

void Host_chain_list::insert(HostEntry *h)
{
    if(this->head == NULL)
    {
        this->head = h;
        this->tail = h;
    }
    else
    {
        this->tail->next = h;
        this->tail = h;
    }
}

HostEntry* Host_chain_list::search(char *hostname)
{
    HostEntry *host_entry_ptr = this->head;
    while( host_entry_ptr )
    {
        if( ! strcasecmp(host_entry_ptr->hostname, hostname) )
            return host_entry_ptr;
        else
            host_entry_ptr = host_entry_ptr->next;
    }
    return NULL;
}

bool Host_chain_list::remove(char *hostname)
{
    HostEntry *host_entry_ptr = this->head;
    while(host_entry_ptr->hostname != NULL)
    {
        if( ! strcasecmp(host_entry_ptr->hostname, hostname))
            return delete_success;
        else
            host_entry_ptr = host_entry_ptr->next;
    }
    return hostname_not_found;
}



//------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------
HostMap::HostMap()
{
    no_of_hostnames = 0;
    host_chain_lists = new Host_chain_list[HASH_TABLE_SIZE]; // making an array of pointers to Host_chain_list objects
}


int HostMap::HashFunc(const char *str)
{
    int c;
    int hash = 5381;

    while (c = *(str++))  // test expression returns false when c = '\0'. The null character equals zero in value on the ascii table.
        hash = ((hash << 5) + hash) + c; //  (hash * 33) + c

    hash = hash % HASH_TABLE_SIZE;
    hash = abs(hash);

    if( !(hash >= 0 && hash < HASH_TABLE_SIZE) )
    {
        printf("\nhas function error : hash value out of range despite trying to contain the range in the has function\n");
        return -1;
    }

    return hash;
}


HostEntry* HostMap::insert(char *hostname, bool cache_enable) // return type is set so only for testing.
{
    unsigned int hash = HashFunc(hostname);
    if( !(hash >= 0 && hash < HASH_TABLE_SIZE) )
    {
        printf("\nhas function error : hash value out of range\n");
        return NULL;
    }

    HostEntry* host_entry_ptr = new HostEntry(hostname, cache_enable);
    this->host_chain_lists[hash].insert(host_entry_ptr);
    this->no_of_hostnames++;

    return host_entry_ptr;
}


HostEntry* HostMap::search(char *hostname) //this return type here is i think crucial for the code that is trying to get to the hostinfoptr.
{
    int hash = HashFunc(hostname);
    HostEntry* host_entry_ptr = host_chain_lists[hash].search(hostname);
    return host_entry_ptr;
}


bool HostMap::remove(char *hostname)
{
    int hash = HashFunc(hostname);
    bool whether_deleted = host_chain_lists[hash].remove(hostname);
    this->no_of_hostnames--;
    return whether_deleted;
}


HostMap::~HostMap()
{
    delete[] host_chain_lists;
}
