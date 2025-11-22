all:
	cc -o sws -ansi -g -Wall -Werror -Wextra -Wformat=2 -Wjump-misses-init \
		-Wlogical-op -Wpedantic -Wshadow sws.c

clean:
	-rm sws
