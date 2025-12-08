# sws

handling of sockets was inspired by some of these examples:
- https://wiki.netbsd.org/examples/socket_programming/
- https://stevens.netmeister.org/631/apue-code/09/streamread.c
- https://stevens.netmeister.org/631/apue-code/09/dualstack-streamread.c
- https://stevens.netmeister.org/631/apue-code/09/one-socket-select-fork.c

# Usage

```
# NetBSD & Linux
make clean & make

# OmniOS
gmake clean & gmake

./sws [-dh] [-c dir] [-i address] [-l file] [-p port] dir
```

# Group Work
### Division of Labor & Contributions
Aya:
- option handling
- sockets: socket, bind, listen, accept
- daemonization
- reading data from client
- request parsing
- logging

Justin:
- HTTP response & headers
- Directory indexing
- GCI execution
- User directory support
- Makefile portability
- tested and verified snapshot met deliverables

### Collaboration
- Discussed tasks and division of labor via Slack
- Managed code and tasks via GitHub
