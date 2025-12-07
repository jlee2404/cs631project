#ifndef _SWS_H_
#define _SWS_H_

int main(int, char **);
void handleConnection(int, struct sockaddr_in6, const char *);
int createSocket(struct addrinfo *);
void handleSocket(int, const char *);
void usage(void);

#endif
