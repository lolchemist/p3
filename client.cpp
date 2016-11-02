#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <strings.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <ev.h>

#define BUFSIZE 1024

class my_io: public ev_io
{
public:
    int serv;
    my_io(int s): serv(s) {}
};

//callback 1
static void input_cb(struct ev_loop *loop, struct ev_io *inf, int revents)
{
    char cache[BUFSIZE];

    scanf("%s", cache);   

    send(static_cast<my_io *>(inf)->serv, cache, BUFSIZE, 0);
}

//callback 2
static void recv_cb(struct ev_loop *loop, struct ev_io *server, int revents)
{
    char cache[BUFSIZE];
    char *buf = (char *) malloc(BUFSIZE);

    int buf_len = BUFSIZE;
    
    if (0 == recv(server->fd, cache, BUFSIZE, 0))
    //server disconnect 
    {
        std::cout << "connection terminated\n";
        ev_io_stop(loop, server);
        delete server;
        close(server->fd);
    }
    else
    //server sent message
    {
        memcpy(buf, cache, BUFSIZE);

        //read message in buffer
        while (recv(server->fd, cache, BUFSIZE, 0) > 0)
        {
            buf_len += BUFSIZE;
            buf = (char *) realloc(buf, buf_len); 

            memcpy(buf + (buf_len - BUFSIZE), cache, BUFSIZE);
        }

        std::cout << "from server: " << buf << std::endl;
    }

    free(buf);
}

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

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    //create socket for client
    int server = socket(AF_INET, SOCK_STREAM, 0);

    //fill server address structure
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //connect to server  
    if (connect(server ,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
      perror("Error connecting:");
      exit(1);
    }

    set_nonblock(server);

    //make loop
    struct ev_loop *loop = ev_default_loop(0);

    //make event on input
    struct ev_io *watcher = new my_io(server);
    ev_init(watcher, input_cb);
    ev_io_set(watcher, 0, EV_READ);
    ev_io_start(loop, watcher);

    //make event on receive
    watcher = new ev_io;
    ev_init(watcher, recv_cb);
    ev_io_set(watcher, server, EV_READ);
    ev_io_start(loop, watcher);

    //run
    while (true)
    {
        ev_loop(loop, 0);
    }
}