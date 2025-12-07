#ifndef _PARSE_H_
#define _PARSE_H_

#include "request.h"

int parseRequest(const char *, struct request *);
int validMethod(const char *);
time_t parseDate(const char *);

#endif
