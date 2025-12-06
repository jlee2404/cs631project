#include <stdio.h>
#include <string.h>

#include "parse.h"

/*
 * checks if a method is GET or HEAD
 * return values
 *  1: it's valid
 *  0: not valid
 */
int
validMethod(const char *method)
{
    if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
        return 1;
    }

    return 0;
}

/*
 * given the request requeststr, this function will parse it into the
 * according variables.
 * return values:
 *  -1: invalid request
 *  0: successfully parsed request
 *
 * for example:
 *  requeststr = "GET / HTTP/1.0\r\n"
 *      "If-Modified-Since Request-Header: Sat, 29 Oct 1994 19:43:31 GMT\r\n"
 *  => method = "GET"
 *  => uri = "/"
 *  => header = "Sat, 29 Oct 1994 19:43:31 GMT"
 */
int
parseRequest(const char *requeststr, struct request *req)
{
    char *line;
    char requestcpy[BUFSIZ] = {0};
    char header[BUFSIZ];
    int n;

    strcpy(requestcpy, requeststr);
    line = strtok(requestcpy, "\r\n");

    sscanf(line, "%s %s HTTP/%f", req->method, req->uri, &(req->version));

    if (!validMethod(req->method)) {
        return -1;
    }

    if (req->version != 0.9 && req->version != 1.0 && req->version != 1.1) {
        return -1;
    }

    /* HTTP/0.9 only supports GET */
    if (req->version == 0.9 && strcmp(req->method, "GET") != 0) {
        return -1;
    }

    for(;;) {
        line = strtok(NULL, "\r\n");
        if (line == NULL) {
            break;
        }

        n = sscanf(line, "If-Modified-Since: %s", header);
        if (n == 1) {
            strcpy(req->if_modified_since_header, header);
        }
    }

    return 0;
}
