CC=	gcc
CFLAGS=	-g -Wall -Werror -Wextra -Wformat=2 -Wjump-misses-init \
	-Wlogical-op -Wshadow
LDFLAGS= -lmagic

IFLAGS= $(shell uname -s | grep -q SunOS && echo '-I/opt/magic/include' || true)
LFLAGS= $(shell uname -s | grep -q SunOS && echo '-L/opt/magic/lib -lsocket -lnsl' || true)

CFLAGS += ${IFLAGS}
LDFLAGS= -lmagic ${LFLAGS}

PROG=	sws
OBJS=	sws.o parse.o

all: ${PROG}

${PROG}: ${OBJS}
	@echo $@ depends on $?
	${CC} ${CFLAGS} ${OBJS} -o ${PROG} ${LDFLAGS}

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f ${PROG} ${OBJS}
