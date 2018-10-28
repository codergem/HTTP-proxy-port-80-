# HTTP-proxy-port-80-
A http proxy that can relay requests to servers and the respective responses to the clients using the one thread per client model. 

Its a program for a simple HTTP proxy. Written for Linux. Uses pthread library and the unix/linux sockets API. Below is a description of the program.


--- A Note : One function, serve_from_cache() in mediator.cpp has not yet been implemented yet. But, intend to do it soon. So, for now, the proxy cannot store a cache of the resources received from server. But, have written dome data structures to handle the same. ---

The different files and their roles : 
 - proxy_srv1.cpp : This contains main(). It implements a concurrent server on the user given interface and port. It starts a thread for every client connection request received using the accept() system call.
 - mediator.cpp : This is where the client request is handled. It reads the client's HTTP request. Then, it decides whether to send it a cached response (if present) or not. If not, this code opens a connection with the actual server to get the fresh resource and relays it to the client.
 - hostmap.cpp : This file implements the functions for a hash table (hashing with chaining) for hostnames. If a hostname is to be served, it must be found in this hash table. Hash table intented to handle presence of large number of hostnames. The HostMap class definition is in csapp.h. 
 - resmap.cpp : This file implements the functions for a hash table for the list of resources under a hostname. If a new URI is seen on a request, it gets added to this table. The new URI is then relayed from the actual server back to the client. When the request is received again, the URI can be found in the table and it can be decided whether to serve from the cache of the resource or not. The ResMap class definition is in csapp.h.
 - content_cacher.h and content_cacher.cpp : These files help implement a cache in heap using a linked list of strings. Though alternatives of either using FIFOs (in linux ipc) or using files in disk were explored, I ended up writing my own class to implement caching.
 - csapp.h : The header file that covers for the sockets library, class definitions for the resmap.cpp and hostmap.cpp files and some other functions used in mediator.cpp.
 - RIO.cpp : Implements functions that handle buffered i/o on sockets. Avoids having to switch to kernel frequently, for use cases where byte level processing is needed.   


Some other details : 
- String parsing done using the KMP algorithm. 
- echo_cli.cpp is a file implementing a simple client. The hostname to query for can be edited in it.
- For testing purpose, three hostnames have been added to proxy_srv1.cpp starting at line 50. One can add another hostname for test the using the same 3 lines of code described.
- terminal_commands is the file with the commands for compilation. To be used for test. One can use telnet instead of the echo_cli.cpp program.


 
Written by : A Jayanth 

Used Randal Bryant's book 'CSAPP' as reference for socket programming, basic IPC and threading details.
