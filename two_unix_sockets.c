/**
 * two_unix_sockets.c
 * This is supposed to be a minimal program to demonstrate
 * a bug I am encountering.
 * This program provides both a server and a client. Which runs
 * depends on the command line argument of either server or client.
 * You should run the server first in one terminal and then run the
 * client. The program should run to completion if everything works.
 * The failure mode I am seeing is that the server's call to accept()
 * hangs.
 */
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAXPENDING 5

void die(const char *msg, int errorno) {
  if (errorno == 0) {
    fprintf(stderr, "%s\n", msg);
  } else {
    fprintf(stderr, "%s: %d=%s\n", msg, errorno, strerror(errorno));
  }
  exit(1);
}

int select_accept(int listener, int client1) {
  fd_set readfds, writefds, exceptfds;
  int width, rc;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  FD_SET(listener, &readfds);
  if (client1 >= 0)
    FD_SET(client1, &readfds);
  width = listener+1;
  rc = select(width, &readfds, &writefds, &exceptfds, 0);
  if (rc == 0) die("select() returned 0", 0);
  if (rc < 0) die("select() returned error", errno);
  if (FD_ISSET(listener, &readfds)) {
    int fd;
    printf("select() says we are ready for accept()\n");
    fd = accept(listener, NULL, NULL);
    if (fd < 0) die("accept() failed after select", errno);
    return fd;
  } else die("select() returned > 0, but did not set our bit", 0);
}

void select_read(int listener, int client1, int client2) {
  fd_set readfds, writefds, exceptfds;
  int width, rc;
  char ibuf[80];
  int nc;
  bool client1_ready;
  bool client2_ready;
  bool listener_ready;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  FD_SET(listener, &readfds);
  FD_SET(client1, &readfds);
  FD_SET(client2, &readfds);
  width = ((client1>client2) ? client1 : client2)+1;
  rc = pselect(width, &readfds, &writefds, &exceptfds, 0, 0);
  if (rc == 0) die("select() returned 0", 0);
  if (rc < 0) die("select() returned error", errno);
  client1_ready = FD_ISSET(client1, &readfds);
  client2_ready = FD_ISSET(client2, &readfds);
  listener_ready = FD_ISSET(listener, &readfds);
  if (rc > 1) {
    printf("pselect reports client1:%s client2:%s listener:%s\n",
      client1_ready ? "ready" : "not",
      client2_ready ? "ready" : "not",
      listener_ready ? "ready" : "not");
  }
  if (client1_ready) {
    nc = read(client1, ibuf, 80);
    if (nc < 0) die("read() from client1 failed", errno);
    printf("Read from %s returned %d bytes\n", "client1", nc);
  }
  if (client2_ready) {
    nc = read(client2, ibuf, 80);
    if (nc < 0) die("read() from client2 failed", errno);
    printf("Read from %s returned %d bytes\n", "client2", nc);
  }
}

void server(const char *svc_name) {
  char ibuf[80];
  int nc;
  struct sockaddr_un local;
  int listener = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0)
    die("socket(AF_UNIX, SOCK_STREAM, 0) failed", errno);
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, svc_name, UNIX_PATH_MAX);
  if (fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK) == -1)
    die("fcntl() failure server()", errno);
  unlink(local.sun_path);
  if (bind(listener, (struct sockaddr *)&local, SUN_LEN(&local)) < 0)
    die("bind() failure in server()", errno);
  if (listen(listener, MAXPENDING) < 0)
    die("listen() failure in server()", errno);
  printf("Server is listening\n");
  int client1 = select_accept(listener, -1);
  int client2 = select_accept(listener, client1);
  printf("Server accepted two client connections\n");
  select_read(listener, client1, client2);
  select_read(listener, client1, client2);
  printf("Read two times\n");
  close(client1);
  close(client2);
  close(listener);
  printf("Server shutting down\n");
}

void client_pselect(int fd) {
  fd_set readfds, writefds, exceptfds;
  int width, rc;
  bool client_ready;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  FD_SET(fd, &readfds);
  width = fd+1;
  rc = pselect(width, &readfds, &writefds, &exceptfds, 0, 0);
  if (rc == 0) die("select() returned 0", 0);
  if (rc < 0) die("select() returned error", errno);
  client_ready = FD_ISSET(fd, &readfds);
  if (!client_ready)
    die("pselect picked someone else!",0);
}

int client_connect(const char *svc_name) {
  struct sockaddr_un local;
  int client = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client < 0)
    die("socket(AF_UNIX, SOCK_STREAM, 0) failed", errno);
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, svc_name, UNIX_PATH_MAX);
  if (fcntl(client, F_SETFL, fcntl(client, F_GETFL, 0) | O_NONBLOCK) == -1)
    die("fcntl() failure server()", errno);
  if (connect(client, (struct sockaddr*)&local, SUN_LEN(&local)) < 0 &&
      errno != EINPROGRESS)
    die("connect() failed", errno);
  client_pselect(client);
  return client;
}

void client(const char *svc_name) {
  int client1, client2;
  printf("Client is starting\n");
  client1 = client_connect(svc_name);
  printf("One connection succeeded\n");
  client2 = client_connect(svc_name);
  printf("Two connections succeeded\n");
  write(client1, "hello", 6);
  sleep(3);
  write(client2, "hello", 6);
  printf("Wrote to both connections\n");
  close(client1);
  close(client2);
}

int main(int argc, char **argv) {
  const char *arg = "server";
  // const char *service = "/var/run/monarch/scopex/tm_gen";
  const char *service = "service";
  if (argc > 1) arg = argv[1];
  if (strcasecmp(arg, "client") == 0) {
    client(service);
  } else if (strcasecmp(arg, "server") == 0) {
    server(service);
  } else {
    fprintf(stderr,"Unrecognized option\n");
    return 1;
  }
  return 0;
}
