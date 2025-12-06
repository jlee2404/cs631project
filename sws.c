#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "parse.h"
#include "sws.h"

#ifndef MAXPENDING
#define MAXPENDING 5
#endif

#ifndef SLEEP
#define SLEEP 5
#endif

/* this was calculated by finding the maximum bytes from printing GMT time, e.g "2019-10-28T12:12:12Z" */
#ifndef TIMEBUFSIZ
#define TIMEBUFSIZ 22
#endif

void
usage(void)
{
    (void)printf("usage: sws [-dh] [-c dir] [-i address] [-l file] [-p port] dir\n");
}

void
logRequest(int logfd, const char *request, const char *response,
        const char *rip, time_t time_now, int status)
{
    char log[BUFSIZ] = {0}, time[TIMEBUFSIZ] = {0}, line[BUFSIZ] = {0};
    struct tm *gmtime_now = gmtime(&time_now);
    int i = 0, responsesz = strlen(response), n;

    strftime(time, TIMEBUFSIZ, "%Y-%m-%dT%H:%M:%SZ", gmtime_now);

    /* get only first line of request */
    while (request[i] != '\r') {
        line[i] = request[i];
        i++;
    }

    if ((n = snprintf(log, BUFSIZ, "%s %s \"%s\" %d %d\n",
        rip, time, line, status, responsesz)) < 0) {
            perror("snprintf");
            return;
    }

    if (write(logfd, log, n) < 0) {
        perror("write");
    }
}

/*
 * create a socket, then bind and listen.
 * loop through the addresses in info until a valid one is found.
 * return values:
 *  -1: no socket was found
 *  >0: the socket
 */
int
createSocket(struct addrinfo *info)
{
    int sock = -1;
    struct addrinfo *p;

    for (p = info; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, SOCK_STREAM, 0)) == -1) {
            continue;
        }

        if (bind(sock, p->ai_addr, p->ai_addrlen) == 0) {
            if (listen(sock, MAXPENDING) < 0) {
                perror("listen");
                exit(EXIT_FAILURE);
            }
            break; /* found the address */
        }

        close(sock);
    }

    return sock;
}

void
handleConnection(int fd, struct sockaddr_in6 client, int logfd)
{
    int rv, status;
    char request[BUFSIZ];
    char claddr[INET6_ADDRSTRLEN];
    char *response;
    const char *rip;
    struct request req;
    time_t time_now = time(NULL);

    memset(&req, 0, sizeof(struct request));

    if ((rv = read(fd, request, BUFSIZ)) <= 0) {
        perror("reading stream message");
        goto exit;
    }

    if ((rip = inet_ntop(PF_INET6, &(client.sin6_addr), claddr, INET6_ADDRSTRLEN)) == NULL) {
        perror("inet_ntop");
        rip = "unkown";
    }
    if (parseRequest(request, &req) == 0) {
        (void)printf("Client: %s\nMethod: %s\nURI: %s\nHeader: %s\n", rip, req.method, req.uri, req.if_modified_since_header);
        status = 200;
        response = "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 19\r\n\r\n"
            "Request valid: OK\r\n";
    } else {
        (void)printf("Client: %s\nParse failed\n", rip);
        status = 400;
        response = "HTTP/1.0 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 15\r\n\r\n"
            "Bad Request\r\n";
    }

    if (logfd >= 0) {
        logRequest(logfd, request, response, rip, time_now, status);
    }

    write(fd, response, strlen(response));

exit:
    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
    /* NOTREACHED */
}

void
handleSocket(int sock, int logfd)
{
    int fd;
    pid_t pid;
    struct sockaddr_in6 client;
    socklen_t length;

    memset(&client, 0, sizeof(client));

    length = sizeof(client);
    if ((fd = accept(sock, (struct sockaddr *)&client, &length)) < 0) {
        perror("accept");
        return;
    }

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { /* child */
        handleConnection(fd, client, logfd);
    }

    /* parent returns */
    if (close(fd) < 0) {
        perror("close");
    }
}

void
reap()
{
    wait(NULL);
}

int
main(int argc, char **argv)
{
    char *cgidir, *dir, *logfile, *address = NULL, *port = "8080";
    int ch, debug = 0, sock, logfd = -1;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC; /* accept both ipv4 and ipv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* wildcards */

    if (signal(SIGCHLD, reap) == SIG_ERR) { /* reap child processes */
        perror("signal");
        exit(EXIT_FAILURE);
    }

    while ((ch = getopt(argc, argv, ":dhc:i:l:p:")) != -1) {
        switch (ch) {
        case 'd':
            debug = 1;
            break;
        case 'h':
            usage();
            return 0;
        case 'c':
            cgidir = optarg;
            break;
        case 'i':
            address = optarg;
            break;
        case 'l':
            logfile = optarg;
            if ((logfd = open(logfile, O_WRONLY | O_APPEND)) < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
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

    if (!debug) {
        if (daemon(0, 0) < 0) {
            perror("daemon");
            exit(EXIT_FAILURE);
        }
    } else { /* debug mode, so log to stdout */
        if (logfd > 0) {
            close(logfd); /* -d overwrites -l */
        }
        logfd = STDOUT_FILENO;
    }

    if (getaddrinfo(address, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }

    if ((sock = createSocket(res)) < 0) {
        perror("createSocket");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);

    for (;;) {
        fd_set ready;
        struct timeval timeout;

        FD_ZERO(&ready);
        FD_SET(sock, &ready);
        timeout.tv_sec = SLEEP;
        timeout.tv_usec = 0;

        if (select(sock + 1, &ready, 0, 0, &timeout) < 0) {
            if (errno != EINTR) {
                perror("select");
            }
        } else if (FD_ISSET(sock, &ready)) {
            handleSocket(sock, logfd);
        }
    }

    (void)dir;
    (void)cgidir;

    close(logfd);
    return 0;
}
