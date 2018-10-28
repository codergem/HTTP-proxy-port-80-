#include "csapp.h"


char http_time_format[] = "%a, %d %b %Y %X %Z";
char expires_header[] = "Expires";

/* ResEntry Class functions definition */
ResEntry::ResEntry(char *uri)
{
    strcpy(this->uri, uri);
    sem_init(&(this->cache_access), 0, 1);
    vary_to_be_checked = false;
}

struct tm *ResEntry::get_current_tm_ptr()
{
    time_t current_time_t;
    time ( &current_time_t );
    struct tm* current_tm_ptr = gmtime ( &current_time_t );
    char temp_str[100];
    strftime(temp_str, sizeof(temp_str), http_time_format, current_tm_ptr);  //converts a tm to a str
    strptime(temp_str, http_time_format, current_tm_ptr); //converts a string to a tm
    return current_tm_ptr;
}

unsigned int ResEntry::current_resource_age()
{
    double current_age = difftime ( mktime(get_current_tm_ptr()), date_of_cache_time_t );
    return (unsigned int) current_age;
}

bool ResEntry::resource_is_stale()
{
    if (difftime (this->expires_time_t, mktime(get_current_tm_ptr()) ) <= 6.0)
        return true;//the resource is considered stale (less than 6 seconds of freshness time left).
}

unsigned int ResEntry::fresh_time_left()
{
    double freshness_time_left = difftime ( this->expires_time_t, mktime(get_current_tm_ptr()) );
    return (unsigned int) freshness_time_left;
}

bool ResEntry::check_vary_match(char* request)
{
    char temp_vary_value[50];
    char* temp1;

    if( temp1 = findFirstOccurrence(this->vary_header, request) )
    {
        header_value_parser(this->vary_header, temp1, temp_vary_value);
        if( ! strcasecmp(temp_vary_value, this->vary_value) )
            return true;
        else
            false;
    }

    else
        return false;
}

bool ResEntry::check_vary_presence(char *request)
{
    char* temp1;
    if( temp1 = findFirstOccurrence(this->vary_header, request) )
       return true;
    else
        return false;
}

void ResEntry::update_resource_status()
{
    char *temp1, *temp2;
    char comma[] = ",";
    char Vary1[] = "Vary";

    this->date_of_cache_time_t = mktime(get_current_tm_ptr());

    //check for vary header
    if( temp1 = findFirstOccurrence(Vary1, this->complete_response) )
    {
        char vary_string[100];
        header_value_parser(Vary1, temp1, vary_string);

        if( temp2 = findFirstOccurrence(comma, vary_string) )
        {
            *temp2 = '\0';
            copy_without_spaces(this->vary_header, vary_string);
        }

        else
            strcpy(this->vary_header, vary_string);

        header_value_parser(this->vary_header, this->complete_response, this->vary_value);

        this->vary_to_be_checked = true;
    }

    if( temp1 = findFirstOccurrence(expires_header, this->complete_response) )
    {
        char expires_str[100];
        header_value_parser(expires_header, temp1, expires_str);
        struct tm expires_tm;
        strptime(expires_str, http_time_format, &expires_tm); // converts a string to a tm
        this->expires_time_t = mktime (&expires_tm);
    }
}


void ResEntry::update_timers()
{
    char last_modified_header[] = "Last-Modified";
    char* ptr;
    struct tm temp_tm = {0};

    char expires_value[30];
    char last_modified_value[30];

    if(ptr = findFirstOccurrence(expires_header, this->complete_response))
    {
        header_value_parser(expires_header, ptr, expires_value, ':');
        strptime(expires_value, http_time_format, &temp_tm); //converts a string to a tm
        this->expires_time_t = mktime(&temp_tm);
    }

    else
    {
        printf("\n wait... no \"Expires:\" header found in the response");
    }

    if(ptr = findFirstOccurrence(last_modified_header, this->complete_request))
    {
        header_value_parser(last_modified_header, ptr, last_modified_value, ':');
        strptime(last_modified_value, http_time_format, &temp_tm); //converts a string to a tm
        this->last_modified_time_t = mktime(&temp_tm);
    }
}



//-------------------------------------------------
Res_chain_list::Res_chain_list()
{
    this->uri_not_found = false;
}

void Res_chain_list::insert(ResEntry *h)
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

ResEntry* Res_chain_list::search(char *uri)
{
    ResEntry *res_entry_ptr = this->head;
    while(res_entry_ptr->uri != NULL)
    {
        if( ! strcasecmp(res_entry_ptr->uri, uri))
            return res_entry_ptr;
        else
            res_entry_ptr = res_entry_ptr->next;
    }
    return NULL;
}

bool Res_chain_list::remove(char *uri)
{
    ResEntry *res_entry_ptr = this->head;
    while(res_entry_ptr->uri != NULL)
    {
         if( ! strcmp(res_entry_ptr->uri, uri) )
            return delete_success;
         else
            res_entry_ptr = res_entry_ptr->next;
    }
    return uri_not_found;
}



//-------------------------------------------------
// ResMap functions definition

ResMap::ResMap()
{
    this->res_lists = new Res_chain_list[HASH_TABLE_SIZE]; // making an array of pointers to Res_chain_list objects
}


int ResMap::HashFunc(const char *str)
{
    int c;
    int hash = 5381;

    while (c = *str++)  // test expression returns false when c = '\0'. Thee null character is equal to zero in ascii.
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return abs(hash % HASH_TABLE_SIZE);
}


ResEntry* ResMap::insert(char *uri) // return type is set so only for testing.
{
    int hash = HashFunc(uri);
    ResEntry* res_entry_ptr = new ResEntry(uri);
    res_lists[hash].insert(res_entry_ptr);
    return res_entry_ptr;
}


ResEntry* ResMap::search(char *uri) //this return type here is i think crucial for the code that is trying to get to the resinfoptr.
{
    int hash = HashFunc(uri);
    ResEntry* res_entry_ptr = res_lists[hash].search(uri);
    return res_entry_ptr;
}


bool ResMap::remove(char *uri)
{
    int hash = HashFunc(uri);
    bool whether_deleted = res_lists[hash].remove(uri);
    return whether_deleted;
}

ResMap::~ResMap()
{
    delete[] res_lists;
}

