#ifndef _SWS_H_
#define _SWS_H_

int main(int, char **);
void handleConnection(int, struct sockaddr_in6, const char *, int, const char *);
int createSocket(struct addrinfo *);
void handleSocket(int, const char *, int, const char *);
void usage(void);
void logRequest(int, const char *, const char *, time_t, int, size_t);
static void formatDate(time_t, char *, size_t);
static const char *guess_mime_type(const char *);
int uriToPath(const char *, const char *, char *, size_t, struct stat *, int *, const char *);

#endif
