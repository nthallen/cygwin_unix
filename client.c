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

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define MAXPENDING 5

void die(const char *msg, int errorno) {
  if (errorno == 0) {
    fprintf(stderr, "%s\n", msg);
  } else {
    fprintf(stderr, "%s: %d=%s\n", msg, errorno, strerror(errorno));
  }
  exit(1);
}

/**
 * @return -1 unless we accept a new client, in which case return the socket
 */
int select_read(int listener, int client1, int client2) {
  fd_set readfds, writefds, exceptfds;
  int width, rc;
  int fd = -1;
  char ibuf[80];
  int nc;
  bool client1_ready;
  bool client2_ready;
  bool listener_ready;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  if (listener >= 0) {
    FD_SET(listener, &readfds);
    width = listener+1;
  }
  if (client1 >= 0) {
    FD_SET(client1, &readfds);
    if (client1+1 > width) width = client1+1;
  }
  if (client2 >= 0) {
    FD_SET(client2, &readfds);
    if (client2+1 > width) width = client2+1;
  }
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
  if (listener_ready) {
    printf("Listener is ready for accept()\n");
    fd = accept(listener, NULL, NULL);
    if (fd < 0) die("accept() failed after select", errno);
    printf("Client accepted\n");
  }
  return fd;
}

void client_pselect(int fd) {
  fd_set readfds, writefds, exceptfds;
  int width, rc;
  bool client_ready;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  FD_SET(fd, &writefds);
  width = fd+1;
  rc = pselect(width, &readfds, &writefds, &exceptfds, 0, 0);
  if (rc == 0) die("select() returned 0", 0);
  if (rc < 0) die("select() returned error", errno);
  client_ready = FD_ISSET(fd, &writefds);
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
  for (;;) {
    if (connect(client, (struct sockaddr*)&local, SUN_LEN(&local)) < 0) {
      if (errno == ENOENT) {
        printf("Waiting for service '%s'\n", svc_name);
        sleep(1);
      } else if (errno == EINPROGRESS) {
        client_pselect(client);
        break;
      } else {
        die("connect() failed before select", errno);
      }
    }
  }
  return client;
}

void client(const char *svc) {
  int client = client_connect(svc);
  const char *auth = "AuthLE scopex miniclt tm_gen/cmd\n";
  write(client, auth, strlen(auth));
  select_read(-1, client, -1);
  printf("Completed authentication\n");
  sleep(1);
  close(client);
  printf("Client shutting down\n");
}

int main(int argc, char **argv) {
  const char *service = "/var/run/monarch/scopex/tm_gen";
  client(service);
  return 0;
}
