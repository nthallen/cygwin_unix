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

#define MAXPENDING 5

void die(const char *msg, int errorno) {
  if (errorno == 0) {
    fprintf(stderr, "%s\n", msg);
  } else {
    fprintf(stderr, "%s: %s\n", msg, strerror(errorno));
  }
  exit(1);
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
  // if (fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK) == -1)
    // die("fcntl() failure server()", errno);
  unlink(local.sun_path);
  if (bind(listener, (struct sockaddr *)&local, SUN_LEN(&local)) < 0)
    die("bind() failure in server()", errno);
  if (listen(listener, MAXPENDING) < 0)
    die("listen() failure in server()", errno);
  printf("Server is listening\n");
  int client1 = accept(listener, NULL, NULL);
  if (client1 < 0) die("accept() #1 failed", errno);
  int client2 = accept(listener, NULL, NULL);
  if (client2 < 0) die("accept() #2 failed", errno);
  printf("Server accepted two client connections\n");
  nc = read(client1, ibuf, 80);
  if (nc < 0) die("read(client1) failed", errno);
  nc = read(client2, ibuf, 80);
  if (nc < 0) die("read(client2) failed", errno);
  printf("Read from both connections\n");
  close(client1);
  close(client2);
  close(listener);
  printf("Server shutting down\n");
}

int client_connect(const char *svc_name) {
  struct sockaddr_un local;
  int client = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client < 0)
    die("socket(AF_UNIX, SOCK_STREAM, 0) failed", errno);
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, svc_name, UNIX_PATH_MAX);
  // if (fcntl(client, F_SETFL, fcntl(client, F_GETFL, 0) | O_NONBLOCK) == -1)
    // die("fcntl() failure server()", errno);
  if (connect(client, (struct sockaddr*)&local, SUN_LEN(&local)) < 0)
    die("connect() failed", errno);
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
  write(client2, "hello", 6);
  printf("Wrote to both connections\n");
  close(client1);
  close(client2);
}

int main(int argc, char **argv) {
  const char *arg = "server";
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
