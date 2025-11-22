#include <arpa/inet.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXPENDING 5

void
usage(void)
{
    (void)printf("usage: sws [-dh] [-c dir] [-i address] [-l file] [-p port] dir\n");
}

/*
 * create a socket, then bind and listen.
 * if info is null, then a socket is created to listen on ipv6 and ipv4
 * equivalent addresses. if info is not null, then loop through the addresses
 * until a valid one is found.
 */
int
createSocket(struct addrinfo *info)
{
    int sock;
    struct addrinfo *p;

    for (p = info; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, SOCK_STREAM, 0)) == -1) {
            continue;
        }

        if (bind(sock, p->ai_addr, p->ai_addrlen) == 0) {
           break; /* found the address */
        }

        close(sock);
    }

    if (listen(sock, MAXPENDING) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return sock;
}

void
handleSocket(int sock)
{
    for (;;) {
        int fd, rv = 1;
        struct sockaddr_in6 client;
        socklen_t length;

        memset(&client, 0, sizeof(client));
        length = sizeof(client);
        if ((fd = accept(sock, (struct sockaddr *)&client, &length)) < 0) {
            perror("accept");
            continue;
        }

        while (rv != 0) {
            char buf[BUFSIZ];
            char *response =
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 5\r\n"
                "\r\n"
                "Hello";

            if ((rv = read(fd, buf, BUFSIZ)) < 0) {
                perror("reading stream message");
                break;
            }
            if (rv > 0) {
                write(1, buf, rv);
            }

            write(fd, response, strlen(response));

            if (rv == 0) {
                (void)printf("Ending connection\n");
                break;
            }
        }

        close(fd);
    }
}

int
main(int argc, char **argv)
{
    char *cgidir, *dir, *logfile, *address, *port = "8080";
    int ch, debug = 0, sock;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC; /* accept both ipv4 and ipv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* wildcards */

    while ((ch = getopt(argc, argv, ":dhc:i:l:p:")) != -1) {
        switch (ch) {
        case 'd':
            debug = 1;
            break;
        case 'h':
            usage();
            return 0;
        case 'c': /* add error checking on directory */
            cgidir = optarg;
            break;
        case 'i':
            address = optarg;
            break;
        case 'l': /* add error checking on directory */
            logfile = optarg;
            break;
        case 'p': /* add error checking on port */
            port = optarg;
            break;
        case '?':
        case ':':
            usage();
            exit(EXIT_FAILURE);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        usage();
        exit(EXIT_FAILURE);
    }

    /* add error checking on directory */
    dir = argv[0];

    /* add error checking on getaddrinfo */
    getaddrinfo(address, port, &hints, &res);

    sock = createSocket(res);
    handleSocket(sock);

    (void)debug;
    (void)dir;
    (void)logfile;
    (void)cgidir;
    return 0;
}
