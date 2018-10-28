#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>

using namespace std;


//-----------------------
class cnread_status
{
    public :
    bool read_all_available_bytes_in_node;
    size_t no_of_bytes_read, bytes_to_read, offset;
    char *dst_str;

    cnread_status();
    void reset_status(char *new_dst_str, size_t bytes_left);
};

//-----------------------
class cache_node
{
    public :
    size_t max_size, next_node_index; //next_node_index is also a measure of the number of bytes written on the node
    char *node_str;
    bool node_full;
    cache_node *next;

    //------------------
    void cnread(cnread_status* node_read_status); //need to mark read_all_available_bytes_in_node to true when we read all bytes possible on this node
    cache_node(size_t node_size = 128);
    size_t cnwrite(char *src_string, size_t bytes_to_be_written);

};

class cache_node_list
{
    public :
    cache_node *head, *tail;
    size_t cache_size_limit, node_size, current_cache_size, size_of_tail_node;
    int number_of_nodes;

    cache_node_list(size_t cache_size_limit_in_KB = 2048, size_t single_node_size = 1024);
    void add_node(cache_node* cache_node_ptr);
    size_t cwrite(char *src_string, size_t bytes_to_be_written);
    int cread(char *dst_str, size_t bytes_to_be_read, bool* no_more_bytes_to_read_in_cache, size_t offset = 0);
    int reset();
};

