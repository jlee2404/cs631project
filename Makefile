CC=	cc
CFLAGS=	-g -Wall -Werror -Wextra -Wformat=2 -Wjump-misses-init \
	-Wlogical-op -Wshadow
LDFLAGS= -lmagic

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
