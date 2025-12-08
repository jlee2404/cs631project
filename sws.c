#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <magic.h>
#include <netdb.h>
#include <pwd.h>
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

#ifndef TIMEBUFSIZ
#define TIMEBUFSIZ 22
#endif

#ifndef MAXUSERNAME
#define MAXUSERNAME 256 /* 255 is classic UNIX username limit */
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

static void
formatDate(time_t t, char *buf, size_t buflen)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, buflen, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

static const char *
guess_mime_type(const char *path) /* FUNCTION NAME */
{
    static magic_t magic_cookie = NULL; /* VARIABLE NAME */

    if (!magic_cookie) {
	magic_cookie = magic_open(MAGIC_MIME_TYPE);
	if (!magic_cookie) {
	    return "application/octet-stream";
	}
	
	if (magic_load(magic_cookie, NULL) != 0) {
	    magic_close(magic_cookie);
	    magic_cookie = NULL;
	    return "application/octet-stream";
	}
    }

    const char *mime = magic_file(magic_cookie, path);
    if (!mime) {
	return "application/octet-stream";
    }

    return mime;
}

int
uriToPath(const char *docroot, const char *uri, char *outpath, 
    size_t outsize, struct stat *statbuf, int *flags_out)
{   
    char candidate[PATH_MAX];
    char resolved[PATH_MAX];
    char realroot[PATH_MAX];

    if (!docroot || !uri || !outpath || outsize == 0) {
	return -1;
    }

    *flags_out = 0;

    if (realpath(docroot, realroot) == NULL) {
	return -1;
    }
    
    if (strstr(uri, "..") != NULL) {
	errno = EACCES;
        return -1;
    }

    if (uri[0] == '~') {
	const char *ptr = uri + 1;
	size_t uname_len = 0;
	while (*ptr && *ptr != '/' && uname_len < MAXUSERNAME) {
	    uname_len++;
	    ptr++;
	}

	if (uname_len == 0 || uname_len >= MAXUSERNAME) {
	    return -1;
	}
	
	char uname[MAXUSERNAME + 1];
	memcpy(uname, uri + 1, uname_len);
	uname[uname_len] = '\0';

	struct passwd *pw = getpwnam(uname);
	if (!pw) {
	    return -1;
	}

	const char *rest = uri + 1 + uname_len;
	if (rest[0] == '\0') {
	    if (snprintf(candidate, sizeof(candidate), "%s/sws", pw->pw_dir) >= 
		(int)sizeof(candidate)) {
		return -1;
	    }
	} else {
	    if (snprintf(candidate, sizeof(candidate), "%s/sws%s", pw->pw_dir, rest) >= 
		(int)sizeof(candidate)) {
		return -1;
	    }
	}
    } else {
	if (uri[0] != '/') {
	    return -1;
	}

	if (snprintf(candidate, sizeof(candidate), "%s%s", docroot, uri) >= 
	    (int)sizeof(candidate)) {
    	    return -1;
	}
    }

    if (realpath(candidate, resolved) == NULL) {
	strncpy(outpath, candidate, outsize);
	outpath[outsize - 1] = '\0';
	return 0;
    }

    size_t rootlen = strlen(realroot);
    if (strncmp(resolved, realroot, rootlen) != 0 ||
	(resolved[rootlen] != '/' && resolved[rootlen] != '\0')) {
	errno = EACCES;
	return -1;
    }

    if (snprintf(outpath, outsize, "%s", resolved) >= (int)outsize) {
	return -1;
    }

    if (stat(outpath, statbuf) == 0) {
	*flags_out |= 1; /* resolved and exists */
	if (S_ISDIR(statbuf->st_mode)) {
	    *flags_out |= 2; /* is direcotry */
	    size_t urilen = strlen(uri);
	    if (uri[urilen-1] != '/') {
		*flags_out |= 4; /* needs trailing slash redirect */
		return 0;
	    }
	    
	    char indexpath[PATH_MAX];
	    if (snprintf(indexpath, sizeof(indexpath), "%s/index.html", outpath) >= 
		(int)sizeof(indexpath)) {
		return -1;
	    }
	    if (stat(indexpath, statbuf) == 0) {
		if (snprintf(outpath, outsize, "%s", indexpath) >= (int)outsize) {
		    return -1;
		}
		*flags_out &= ~2; /* file now */
		*flags_out |= 1;
	    }
	}
    }

    return 0;
}

void
handleConnection(int fd, struct sockaddr_in6 client, const char *dir, int logfd)
{
    int rd, status;
    int flags = 0;
    int wrote_direct = 0;
    int filefd = -1;
    char request[BUFSIZ];
    char header[BUFSIZ];
    char claddr[INET6_ADDRSTRLEN];
    char *response;
    const char *rip;
    const char *mime = NULL;
    struct request req;
    time_t time_now = time(NULL);

    memset(&req, 0, sizeof(req));
    
    if ((rd = read(fd, request, BUFSIZ-1)) <= 0) {
        perror("reading stream message");
        goto exit;
    }
    request[rd] = '\0';

    if ((rip = inet_ntop(PF_INET6, &(client.sin6_addr), claddr, INET6_ADDRSTRLEN)) == NULL) {
        perror("inet_ntop");
        rip = "unkown";
    }

    if (parseRequest(request, &req) != 0) {
	if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "HEAD") != 0) {
	    status = 501;
	    response = "HTTP/1.0 501 Not Implemented\r\n"
            	"Content-Type: text/plain\r\n"
	    	"Content-Length: 17\r\n\r\n"
    	    	"Not Implemented\r\n";
    	} else {
	    status = 400;
  	    response = "HTTP/1.0 400 Bad Request\r\n"
            	"Content-Type: text/plain\r\n"
	    	"Content-Length: 13\r\n\r\n"
    	   	 "Bad Request\r\n";
	}

	goto send_response;
    }

    char fullpath[PATH_MAX];
    struct stat sb;

    if (uriToPath(dir, req.uri, fullpath, sizeof(fullpath), &sb, &flags) < 0) {
	status = 403;
	response = "HTTP/1.0 403 Forbidden\r\n"
	    "Content-Type: text/plain\r\n"
	    "Content-Length: 11\r\n\r\n"
	    "Forbidden\r\n";
	goto send_response;
    }

    if (!(flags & 1)) {
	status = 404;
	response = "HTTP/1.0 404 Not Found\r\n"
       	    "Content-Type: text/plain\r\n"
	    "Content-Length: 11\r\n\r\n"
	    "Not Found\r\n";
	goto send_response;
    }

    if (flags & 4) {
	char uri_with_slash[PATH_MAX];
	size_t urilen = strlen(req.uri);

	if (urilen > 0 && req.uri[urilen - 1] == '/') {
	    snprintf(uri_with_slash, sizeof(uri_with_slash), "%s", req.uri);
	} else {
	    snprintf(uri_with_slash, sizeof(uri_with_slash), "%s/", req.uri);
	}

	status = 301;
	snprintf(header, sizeof(header), 
	    "HTTP/1.0 301 Moved Permanently\r\n"
	    "Location: %s\r\n"
	    "Content-Length: 0\r\n\r\n",
	    uri_with_slash);
	response = header;
	goto send_response;
    }

    char dateBuf[64]; /* MAGIC NUMBER */
    char lastModBuf[64]; /* MAGIC NUMBER */
    if (req.ims_time > 0 && sb.st_mtime <= req.ims_time) {
	formatDate(time_now, dateBuf, sizeof(dateBuf));
	formatDate(sb.st_mtime, lastModBuf, sizeof(lastModBuf));

	status = 304;
	snprintf(header, sizeof(header),
	    "HTTP/1.0 304 Not Modified\r\n"
	    "Date: %s\r\n"
	    "Server: sws/1.0\r\n"
	    "Last-Modified: %s\r\n"
	    "Content-Length: 0\r\n\r\n",
	    dateBuf, lastModBuf);
	response = header;
	goto send_response;
    }

    if ((flags & 2)) {
	DIR *dirp = opendir(fullpath);
    	if (!dirp) {
	    status = 403;
	    response = "HTTP/1.0 403 Forbidden\r\n"
	        "Content-Type: text/plain\r\n"
    		"Content-Length: 11\r\n\r\n"
		"Forbidden\r\n";
	    goto send_response;
	}

	char body[8192]; /* MAGIC NUMBER */
	int body_len = snprintf(body, sizeof(body),
	    "<html><head><title>Index of %s</title></head>"
	    "<body><h1>Index of %s</h1><ul>", req.uri, req.uri);

	struct dirent *dp;
	while((dp = readdir(dirp)) != NULL) {
 	    if (dp->d_name[0] == '.') {
		continue;
	    }
	    body_len += snprintf(body + body_len, sizeof(body) - body_len,
		"<li><a href=\"%s%s\">%s</a></li>",
		req.uri, dp->d_name, dp->d_name);
	}
	closedir(dirp);

	body_len += snprintf(body + body_len, sizeof(body) - body_len,
	    "</ul></body></html>");

	formatDate(time_now, dateBuf, sizeof(dateBuf));
	formatDate(sb.st_mtime, lastModBuf, sizeof(lastModBuf));

	status = 200;
	snprintf(header, sizeof(header),
	    "HTTP/1.0 200 OK\r\n"
	    "Date: %s\r\n"
	    "Server: sws/1.0\r\n"
	    "Last-Modified: %s\r\n"
	    "Content-Type: text/html\r\n"
	    "Content-Length: %d\r\n\r\n%s",
	    dateBuf, lastModBuf, body_len, body);
	response = header;
	goto send_response;
    }

    filefd = open(fullpath, O_RDONLY);
    if (filefd < 0) {
	status = 403;
	response = "HTTP/1.0 403 Forbidden\r\n"
	    "Content-Type: text/plain\r\n"
     	    "Content-Length: 11\r\n\r\n"
	    "Forbidden\r\n";
	goto send_response;
    }

    formatDate(time_now, dateBuf, sizeof(dateBuf));
    formatDate(sb.st_mtime, lastModBuf, sizeof(lastModBuf));
    mime = guess_mime_type(fullpath);

    status = 200;
    snprintf(header, sizeof(header),
	"HTTP/1.0 200 OK\r\n"
	"Date: %s\r\n"
	"Server: sws/1.0\r\n"
	"Last-Modified: %s\r\n"
	"Content-Type: %s\r\n"
	"Content-Length: %jd\r\n\r\n",
	dateBuf, lastModBuf, mime, (intmax_t)sb.st_size);

    write(fd, header, strlen(header));

    if (strcmp(req.method, "GET") == 0) {
	char buf[BUFSIZ];
	ssize_t n;
	while ((n = read(filefd, buf, sizeof(buf))) > 0) {
	    write(fd, buf, n);
	}
    }

    if (close(filefd) < 0) {
	perror("close");
	exit(EXIT_FAILURE);
    }
    response = header;
    wrote_direct = 1;

send_response:
    if (!wrote_direct && response) {
        if (write(fd, response, strlen(response)) < 0) {
	    perror("write");
        }
    }

    if (logfd >= 0) {
	logRequest(logfd, request, response, rip, time_now, status);
    }

exit:
    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    return;
    /* NOTREACHED */
}

void
handleSocket(int sock, const char *dir, int logfd)
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
        handleConnection(fd, client, dir, logfd);
	_exit(EXIT_SUCCESS);
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
    } else {
	if (logfd > 0) {
	    close(logfd);
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
            handleSocket(sock, dir, logfd);
        }
    }

    (void)dir;
    (void)logfile;
    (void)cgidir;

    close(logfd);
    return 0;
}
