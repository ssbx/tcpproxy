#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#ifdef __linux__
#include <sys/epoll.h>
#else
#include <sys/time.h>
#include <sys/event.h>
#endif // __linux__

#define MAX_EVENTS 10
#define MAX_CLIENTS 100

typedef struct
{
    int in;
    int out;
} TcpLink;

void
test_in(TcpLink *tcp_link)
{
    int msg_max = 30, n;
    char remote_says[msg_max + 1];
    n = read(tcp_link->in, remote_says, msg_max);
    remote_says[n] = '\0';

    write(tcp_link->in, remote_says, strlen(remote_says));
    close(tcp_link->in);
    free(tcp_link);
}

int
main(int argc, char** argv)
{
    int srv_fd, opt_val, epoll_fd, ready_count, n, l, cli_port, cli_fd, one = 1;
    struct epoll_event events[MAX_EVENTS];
    struct epoll_event evt;
    struct sockaddr_in srv_saddr, cli_saddr;
    uint32_t cli_ip;
    TcpLink *tcp_link;
    socklen_t sock_len;

    if (argc < 2)
    {
        fprintf(stderr, "No enought arguments\n");
        exit(EXIT_FAILURE);
    }

    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0)
        perror("Error on create socket: ");

    // Reuse flag
    if (getsockopt(srv_fd, SOL_SOCKET, SO_TYPE,
                   (void*) &opt_val, &sock_len) == 0)
    {
        opt_val = 1;
        setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR,
                   (char*) &opt_val, sizeof opt_val);
    }

    // Bind
    memset(&srv_saddr, 0, sizeof (srv_saddr));
    srv_saddr.sin_family = AF_INET;
    srv_saddr.sin_port = htons(atoi(argv[1]));
    if (bind(srv_fd, (struct sockaddr*) &srv_saddr, sizeof srv_saddr) < 0)
        perror("Error on bind: ");

    // Listen
    listen(srv_fd, MAX_CLIENTS);

    if ((fcntl(srv_fd, F_SETFL, fcntl(srv_fd, F_GETFL) | O_NONBLOCK)) < 0)
        perror("Error on fcntl: ");

    // Epoll
    if ((epoll_fd = epoll_create1(0)) < 0)
        perror("Error in epoll create: ");

    evt.data.fd = srv_fd;
    evt.events = EPOLLIN;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, srv_fd, &evt) != 0)
        perror("Error in epoll ctl: ");

    // Main
    while (1)
    {
        printf("Entering while\n");
        // Wait indefinitely (-1)
        ready_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (ready_count < 0)
            perror("Error in epoll wait: ");

        // Handle events
        for (n = 0; n < ready_count; n++)
        {
            // Tcp connect event
            if (events[n].data.fd == srv_fd)
            {
                // Accept
                l = sizeof cli_saddr;
                if ((cli_fd = accept(srv_fd, (void*) &cli_saddr, &l)) < 0)
                    perror("Error in accept: ");

                // Log
                //cli_ip = (unsigned char *) &cli_saddr.sin_addr;
                cli_ip = cli_saddr.sin_addr.s_addr;
                cli_port = ntohs(cli_saddr.sin_port);
                printf("Connexion from client %i %i\n", cli_ip, cli_port);

                // Configure
                setsockopt(cli_fd, IPPROTO_TCP, TCP_NODELAY,
                           (char*) &one, sizeof one);
                if ((fcntl(cli_fd, F_SETFL,
                           fcntl(cli_fd, F_GETFL) | O_NONBLOCK)) < 0)
                    perror("Error in client fcntl: ");

                // Epoll
                tcp_link = malloc(sizeof (TcpLink));
                tcp_link->in = cli_fd;
                tcp_link->out = -1;
                evt.events = EPOLLIN | EPOLLET;
                evt.data.ptr = tcp_link;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_fd, &evt) < 0)
                    perror("Error in client epoll ctl: ");
            }
            else // Socket ready event
            {
                tcp_link = (TcpLink*) events[n].data.ptr;
                test_in(tcp_link);
            }

        }

    }
    exit(EXIT_SUCCESS);

}
