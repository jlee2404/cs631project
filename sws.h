#ifndef _SWS_H_
#define _SWS_H_

int main(int, char **);
void handleConnection(int, struct sockaddr_in6, const char *, int);
int createSocket(struct addrinfo *);
void handleSocket(int, const char *, int);
void usage(void);
void logRequest(int, const char *, const char *, const char *, time_t, int);

#endif
