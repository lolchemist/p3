#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <list>
#include <netdb.h>
#include <ev.h>

#define BUFSIZE 1024

//clients
std::list<int> clients;

class my_io: public ev_io
{
public:
    int val;
};

int set_nonblock(int fd)
{
    int flags;
    #ifdef O_NONBLOCK
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    #endif
    flags = 1;
    return ioctl(fd, FIONBIO, &flags);
}

//callback 1
static void read_cb(struct ev_loop *loop, struct ev_io *client, int revents)
{
    char cache[BUFSIZE];
    char *buf = (char *) malloc(BUFSIZE);

    int buf_len = BUFSIZE;
    
    if (0 == recv(client->fd, cache, BUFSIZE, 0))
    {
        clients.remove(client->fd);
        std::cout << "connection terminated\n";
        ev_io_stop(loop, client);
        delete static_cast<my_io *>(client);
        close(client->fd);
    }
    else
    {
        memcpy(buf, cache, BUFSIZE);

        //read message in buffer
        while (recv(client->fd, cache, BUFSIZE, 0) > 0)
        {
            buf_len += BUFSIZE;
            buf = (char *) realloc(buf, buf_len); 

            memcpy(buf + (buf_len - BUFSIZE), cache, BUFSIZE);
        }

        std::cout << buf << std::endl;
        send(client->fd, buf, BUFSIZE, 0);
    }

    free(buf);
}

//callback 2
static void accept_cb(struct ev_loop *loop, struct ev_io *server, int revents)
{
    int client_socket = accept(server->fd, 0, 0);
    set_nonblock(client_socket);
    clients.push_front(client_socket);

    struct ev_io *watcher = new my_io;
    ev_init(watcher, read_cb);
    ev_io_set(watcher, client_socket, EV_READ);
    ev_io_start(loop, watcher);

    std::cout << "accepted connection\n";
}

int main(int argc, char **argv)
{
    //return 1;
    char *host, *port;
    int c;
    if (argc != 2) {
        std::cerr << "Not enough arguments\n 1 - port (8001 i.e.)\n";
        return 1;
    }

    //create server
    int server = socket(AF_INET, SOCK_STREAM, 0);

    //fill server struct 
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind server
    if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Error in bind\n";
        exit(1);
    }

    if (listen(server, SOMAXCONN) < 0)
    {
        std::cerr << "Error in listen\n";
        exit(1);
    }

    struct ev_loop *loop = ev_default_loop(0);

    //ent for accept
    struct ev_io *watcher = new my_io;
    ev_init(watcher, accept_cb);
    ev_io_set(watcher, server, EV_READ);
    ev_io_start(loop, watcher);

    printf("Waiting for clients...\n");

    //go
    while (true)
    {
        ev_loop(loop, 0);
    }

    shutdown(server, SHUT_RDWR);
    close(server);
}