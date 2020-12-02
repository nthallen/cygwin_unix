CFLAGS = -g -Wall
.PHONY : all
all : two_unix_sockets client

two_unix_sockets : two_unix_sockets.c

client : client.c
