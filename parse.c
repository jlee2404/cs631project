#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "parse.h"

time_t
parseDate(const char *date_str)
{
    if (!date_str || !*date_str) {
	return 0;
    }

    struct tm time;
    char *ret;

    const char *formats[] = {
	"%a, %d %b %Y %H:%M:%S GMT",
	"%A, %d-%b-%y %H:%M:%S GMT",
	"%a %b %d %H:%M:%S %Y"
    };
    
    int i;
    for (i = 0; i< 3; i++) {
	memset(&time, 0, sizeof(time));
	ret = strptime(date_str, formats[i], &time);
	if (ret != NULL) {
		return timegm(&time);
	}
    }

    return 0;
}

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
    if (!requeststr || !req) {
        return -1;
    }

    char line[BUFSIZ];
    const char *ptr = requeststr;
    char *endCurrentLine;

    endCurrentLine = strstr(ptr, "\r\n");
    if (!endCurrentLine) {
	return -1;
    }

    size_t len = endCurrentLine - ptr;
    if (len >= sizeof(line)) {
	len = sizeof(line) - 1;
    }

    strncpy(line, ptr, len);
    line[len] = '\0';
    ptr = endCurrentLine + 2;
    
    int result;
    if ((result = sscanf(line, "%s %s HTTP/%f",
	req->method, req->uri, &(req->version))) != 3) {
	return -1;
    }

    if (!validMethod(req->method)) {
        return -1;
    }
    
    /* downgrades communications to 1.0 */
    if (req->version > 1.099 && req->version < 1.101) {
	req->version = 1.0;
    }

    if (req->version != 0.9 && req->version != 1.0 && req->version != 1.1) {
        return -1;
    }

    /* HTTP/0.9 only supports GET */
    if (req->version == 0.9 && strcmp(req->method, "GET") != 0) {
        return -1;
    }

    req->if_modified_since[0] = '\0';
    req->ims_time = 0;

    while (*ptr && strncmp(ptr, "\r\n", 2) != 0) {
	endCurrentLine = strstr(ptr, "\r\n");
	if (!endCurrentLine) {
	    break;
	}

	len = endCurrentLine - ptr;
	if (len >= sizeof(line)) {
	    len = sizeof(line) - 1;
	}

    	strncpy(line, ptr, len);
    	line[len] = '\0';
   	ptr = endCurrentLine + 2;

	if (strncasecmp(line, "If-Modified-Since:", 18) == 0) {
	    const char * val = line + 18;
      	    while (isspace((unsigned char)*val)) {
		val++;
  	    }
	    strncpy(req->if_modified_since, val, BUFSIZ-1);
	    req->if_modified_since[BUFSIZ-1] = '\0';
	    req->ims_time = parseDate(req->if_modified_since);
	}
    }

    return 0;
}
