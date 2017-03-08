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

#define S_LINK_INITIAL 0;
#define S_READ_REQUEST 1;

typedef struct {
    int in;
    int out;
    int state;
} TcpLink;

int EPOLL_FD;
int SRV_FD;

void
handle_in(TcpLink *tcp_link)
{

    int msg_max = 30, n;
    char remote_says[msg_max + 1];

    /*
     * TODO use a state machine machanism.
     */
    n = read(tcp_link->in, remote_says, msg_max);
    remote_says[n] = '\0';

    write(tcp_link->in, remote_says, strlen(remote_says));

    close(tcp_link->in);
    free(tcp_link);

    return;

}

void
handle_out(TcpLink *tcp_link)
{

    /*
     * TODO use a state machine machanism.
     */
    return;

}

void
handle_err(TcpLink *tcp_link)
{

    /*
     * This one is easy. On error, close both side of the link.
     */
    close(tcp_link->in);
    free(tcp_link);

}

void
handle_new(int srv_fd)
{

    int s, cli_fd, cli_port, one = 1;

    struct sockaddr_in cli_saddr;
    struct epoll_event evt;

    uint32_t cli_ip;
    TcpLink *tcp_link;

    /*
     * Accept new client
     */
    s = sizeof cli_saddr;
    if ((cli_fd = accept(srv_fd, (void*) &cli_saddr, &s)) < 0)
        perror("Error in accept: ");


    /*
     * Extract ip/port info, presently for log purpose only.
     */
    cli_ip = cli_saddr.sin_addr.s_addr;
    cli_port = ntohs(cli_saddr.sin_port);

    printf("Connexion from client %i %i\n", cli_ip, cli_port);


    /*
     * Configure socket, O_NONBLOCK
     */
    setsockopt(cli_fd, IPPROTO_TCP, TCP_NODELAY,
            (char*) &one, sizeof one);

    if ((fcntl(cli_fd, F_SETFL,
            fcntl(cli_fd, F_GETFL) | O_NONBLOCK)) < 0)
        perror("Error in client fcntl: ");


    /*
     * Prepare epoll_event for epoll fd,
     */
    tcp_link = malloc(sizeof (TcpLink));

    tcp_link->in = cli_fd;
    tcp_link->out = -1;
    tcp_link->state = S_LINK_INITIAL;

    evt.events = EPOLLIN | EPOLLOUT | EPOLLET;
    evt.data.ptr = tcp_link;


    /*
     * Register the new socket to the epoll fd
     */
    if (epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, cli_fd, &evt) < 0)
        perror("Error in client epoll ctl: ");

    return;

}

int
main(int argc, char** argv)
{

    int opt_val, event_count, n;
    socklen_t sock_len;

    struct epoll_event evt, events[MAX_EVENTS];
    struct sockaddr_in srv_saddr;


    if (argc < 2) {
        fprintf(stderr, "No enought arguments\n");
        exit(EXIT_FAILURE);
    }


    /*
     * Create the server fd.
     */
    SRV_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (SRV_FD < 0)
        perror("Error on create socket: ");


    /*
     * Configure it (reuse)
     */
    if (getsockopt(SRV_FD, SOL_SOCKET, SO_TYPE,
            (void*) &opt_val, &sock_len) == 0) {

        opt_val = 1;
        setsockopt(SRV_FD, SOL_SOCKET, SO_REUSEADDR,
                (char*) &opt_val, sizeof opt_val);
    }



    /*
     * Bind to port
     */
    memset(&srv_saddr, 0, sizeof (srv_saddr));

    srv_saddr.sin_family = AF_INET;
    srv_saddr.sin_port = htons(atoi(argv[1]));

    if (bind(SRV_FD, (struct sockaddr*) &srv_saddr, sizeof srv_saddr) < 0)
        perror("Error on bind: ");


    /*
     * Listen, and set fd non blocking. We will use epoll.
     */
    listen(SRV_FD, MAX_CLIENTS);

    if ((fcntl(SRV_FD, F_SETFL, fcntl(SRV_FD, F_GETFL) | O_NONBLOCK)) < 0)
        perror("Error on fcntl: ");


    /*
     * Create the epoll fd.
     */
    if ((EPOLL_FD = epoll_create1(0)) < 0)
        perror("Error in epoll create: ");


    /*
     * Add the server socket fd to the epoll fd.
     */
    evt.data.fd = SRV_FD;
    evt.events = EPOLLIN;

    if (epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, SRV_FD, &evt) != 0)
        perror("Error in epoll ctl: ");


    /*
     * We are now ready to accept client connexions.
     *
     * TODO: Define a state machine that efficiently handle concurrent
     * bidirectionnal communication.
     *
     * TODO: This is where we will want to optimize (search for "C state
     * machine with function pointers").
     *
     * TODO: start defined number of threads (with correct affinity) and
     * their own epoll fd.
     */
    while (1) {

        /*
         * Wait indefinitely fo event(s)
         */
        event_count = epoll_wait(EPOLL_FD, events, MAX_EVENTS, -1);
        if (event_count < 0)
            perror("Error in epoll wait: ");


        /*
         * Got some.
         */
        for (n = 0; n < event_count; n++) {

            evt = events[n];

            /*
             * Event concern the server fd. It is a client connexion.
             * No need to go further.
             */
            if (evt.data.fd == SRV_FD) {
                handle_new(SRV_FD);
                continue;
            }


            /*
             * Handle both EPOLLERR and EPOOHUP which will close the socket.
             * No need to go further.
             */
            if ((evt.events & (EPOLLERR | EPOLLHUP)) > 0) {
                handle_err(evt.data.ptr);
                continue;
            }


            /*
             * Handle ready read(in)/write(out) fd events.
             * TODO: maybe the same call, and use a state to decide what
             * we should do.
             */
            if ((evt.events & EPOLLIN) > 0)
                handle_in(evt.data.ptr);

            if ((evt.events & EPOLLOUT) > 0)
                handle_out(evt.data.ptr);

        }

    }
    exit(EXIT_SUCCESS);

}
