CFLAGS = -g -Wall
CXXFLAGS = -g -Wall
.PHONY : all
all : two_unix_sockets client rapid_connects

two_unix_sockets : two_unix_sockets.c

client : client.c

rapid_connects : rapid_connects.cc
