#ifndef _REQUEST_H_
#define _REQUEST_H_

#include <limits.h>
#include <time.h>

#ifndef METHODSZ
#define METHODSZ 5 /* enough for GET/HEAD + \0 */
#endif

struct request {
    char method[METHODSZ];
    char uri[PATH_MAX];
    float version;
    char header[BUFSIZ];
    char if_modified_since[BUFSIZ];
    time_t ims_time;
};

#endif
