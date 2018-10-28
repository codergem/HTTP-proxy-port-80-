#include "content_cacher.h"

//-----------------------------------------------------------------------------------------------------------------------------
    cnread_status::cnread_status()
    {
        read_all_available_bytes_in_node = false;
        no_of_bytes_read = 0;
        offset = 0;
    }

    void cnread_status::reset_status(char *new_dst_str, size_t bytes_left)
    {
        dst_str = new_dst_str;
        bytes_to_read = bytes_left;
        read_all_available_bytes_in_node = false;
        no_of_bytes_read = 0;
        offset = 0;
    }


//-----------------------------------------------------------------------------------------------------------------------------
    cache_node::cache_node(size_t node_size)
    {
        this->max_size = node_size;
        this->node_str = (char*) malloc(sizeof(char)*node_size);
        this->node_full = false;
        this->next_node_index = 0;
    }

    size_t cache_node::cnwrite(char *src_string, size_t bytes_to_be_written)
    {
        size_t bytes_written = 0;
        while( (this->next_node_index < max_size) && (bytes_written < bytes_to_be_written))
        {
            node_str[this->next_node_index] =  src_string[bytes_written];
            next_node_index++;
            bytes_written++;
        }

        if(next_node_index == max_size)
            node_full = true;

        return bytes_written;
    }


    void cache_node::cnread(cnread_status* node_read_status) //need to mark read_all_available_bytes_in_node to true when we read all bytes possible on this node
    {
        size_t i = 0;
        char* node_byte_ptr = &(this->node_str[node_read_status->offset]);

        while( (i < node_read_status->bytes_to_read) && ( (node_read_status->offset)+i < next_node_index))
        {
            node_read_status->dst_str[i] = node_byte_ptr[i];
            i++;
        }

        if ( (node_read_status->offset)+i == next_node_index)
        {
            node_read_status->read_all_available_bytes_in_node = true;
        }

        node_read_status->no_of_bytes_read = i;

        return;
    }


//-----------------------------------------------------------------------------------------------------------------------------
    cache_node_list::cache_node_list(size_t cache_size_limit_in_KB, size_t single_node_size)
    {
        this->cache_size_limit = cache_size_limit_in_KB;
        this->node_size = single_node_size;
        this->current_cache_size = 0;
        this->number_of_nodes = 0;
    }

    int cache_node_list::reset()
    {
        cache_node *curr_node = this->head;
        cache_node *next_node;
        if( this->head )
        {
            int counter = 0;
            while( curr_node )
            {
                next_node = curr_node->next;
                delete curr_node;
                counter++;
            }
            return counter;
        }
        else
            return 0;
    }

    void cache_node_list::add_node(cache_node* cache_node_ptr)
    {
        if(!(this->head))
        {
            this->head = cache_node_ptr;
            this->tail = cache_node_ptr;
            this->number_of_nodes++;
        }
        else
        {
            this->tail->next = cache_node_ptr;
            this->tail = cache_node_ptr;
            this->number_of_nodes++;
        }
    }

    size_t cache_node_list::cwrite(char *src_string, size_t bytes_to_be_written)
    {
        if(!(this->head))
        {
                cache_node* cache_node_ptr = new cache_node(node_size);
                this->add_node(cache_node_ptr);
        }

        size_t  bytes_remaining = bytes_to_be_written, bytes_written = 0, n;
        char *next_src_str_ptr = src_string;
        while(bytes_written < bytes_to_be_written)
        {
                n = tail->cnwrite(next_src_str_ptr, bytes_remaining);
            bytes_written += n;
                this->current_cache_size += n;
                bytes_remaining -= n;
                next_src_str_ptr += n;

            if(tail->node_full)
                {
                cache_node *cache_node_ptr = new cache_node(node_size);
                    this->add_node(cache_node_ptr);
                }
        }

        return bytes_written;
    }


    int cache_node_list::cread(char *dst_str, size_t bytes_to_be_read, bool* no_more_bytes_to_read_in_cache, size_t offset)
    {
        char *current_dst_str_ptr = dst_str;
        size_t no_of_bytes_read = 0, bytes_left = bytes_to_be_read;
        *no_more_bytes_to_read_in_cache = false;

        int no_of_nodes_to_bypass, n; //n is some temporary variable. First used while finding the starting node top reads based on offset. Later used to contain number of bytes read by cnread()
        no_of_nodes_to_bypass = offset/(this->node_size); //the number of nodes to cross
        n = no_of_nodes_to_bypass;
        cache_node *cache_node_ptr = this->head;

        while(n > 0)  //finding the first node according to offset
        {
            if(cache_node_ptr->next)
            {
                cache_node_ptr = cache_node_ptr->next;
                n--;
            }
            else
            {
                printf("\noffset sent to cache crosses cache_size");
                *no_more_bytes_to_read_in_cache = true;
                return no_of_bytes_read;
            }
        }

        cnread_status node_read_status; //will be used while calling cnread()
        node_read_status.bytes_to_read = bytes_to_be_read;
        node_read_status.offset = offset % (this->node_size);
        node_read_status.dst_str = dst_str;
        node_read_status.read_all_available_bytes_in_node = false;

        while (no_of_bytes_read < bytes_to_be_read)
        {
            cache_node_ptr->cnread(&node_read_status);
            n = node_read_status.no_of_bytes_read;
            no_of_bytes_read += n;
            current_dst_str_ptr += n;
            bytes_left -= n;

            if(no_of_bytes_read < bytes_to_be_read)
            {
                if(node_read_status.read_all_available_bytes_in_node) //will be the case even if node is not full
                {
                    if(cache_node_ptr->next) //While will execute again after this.
                    {
                        cache_node_ptr = cache_node_ptr->next;
                        node_read_status.reset_status(current_dst_str_ptr, bytes_left);
                    }
                    else
                    {
                        *no_more_bytes_to_read_in_cache = true;
                        return no_of_bytes_read;
                    }
                }

                else
                {
                    printf("\nunexpected end of read at a cache_node, need to handle this");
                    return no_of_bytes_read;
                }
            }
        }
    }

    void copy_str(char* s1, char *s2, int length)
    {
        for (int i = 0; i < length-1 ; i++)
        {
            s2[i] = s1[i];
        }
        s2[length -1] = '\0';
    }

