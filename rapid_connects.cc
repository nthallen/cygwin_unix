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

class nb_app {
  public:
    nb_app();
    ~nb_app();
    void setup_listener(const char *svc);
    int client_connect(const char *svc_name);
    void single_event();
    void die(const char *msg, int errorno);
    const char *service;
    int listener;
    int client1;
    int client2;
    bool client1_connected;
    bool client2_connected;
    fd_set readfds, writefds, exceptfds;
};

nb_app::nb_app() :
    service(0),
    listener(-1),
    client1(-1),
    client2(-1),
    client1_connected(false),
    client2_connected(false) {
}

nb_app::~nb_app() {
  if (listener > 0) {
    close(listener);
    listener = -1;
  }
  if (client1 > 0) {
    close(client1);
    client1 = -1;
  }
  if (client2 > 0) {
    close(client2);
    client2 = -1;
  }
  if (service) {
    unlink(service);
    printf("Deleting service socket '%s'\n", service);
  }
}

void nb_app::setup_listener(const char *svc) {
  service = svc;
  struct sockaddr_un local;
  listener = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0)
    die("socket(AF_UNIX, SOCK_STREAM, 0) failed", errno);
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, svc, UNIX_PATH_MAX);
  if (fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK) == -1)
    die("fcntl() failure in new_listener()", errno);
  unlink(local.sun_path);
  if (bind(listener, (struct sockaddr *)&local, SUN_LEN(&local)) < 0)
    die("bind() failure in new_listener()", errno);
  if (listen(listener, MAXPENDING) < 0)
    die("listen() failure in new_listener()", errno);
  printf("Server '%s' is listening\n", svc);
}

int nb_app::client_connect(const char *svc_name) {
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
        break;
      } else {
        die("connect() failed before select", errno);
      }
    } else {
      die("connect() apparently succeeded immediately", 0);
    }
  }
  return client;
}

void nb_app::single_event() {
  int rc;
  char ibuf[80];
  int nc;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  int width = 0;
  if (listener >= 0) {
    FD_SET(listener, &readfds);
    if (listener+1 > width) width = listener+1;
  }
  if (client1 >= 0) {
    FD_SET(client1, &readfds);
    if (!client1_connected) FD_SET(client1, &writefds);
    if (client1+1 > width) width = client1+1;
  }
  if (client2 >= 0) {
    FD_SET(client2, &readfds);
    if (!client2_connected) FD_SET(client2, &writefds);
    if (client2+1 > width) width = client2+1;
  }
  rc = pselect(width, &readfds, &writefds, &exceptfds, 0, 0);
  if (rc == 0) die("pselect() returned 0", 0);
  if (rc < 0) die("pselect() returned error", errno);
  if (client1 >= 0) {
    if (!client1_connected) {
      if (FD_ISSET(client1, &writefds)) {
        client1_connected = true;
        printf("client1 connected\n");
      }
    } else if (FD_ISSET(client1, &readfds)) {
      nc = read(client1, ibuf, 80);
      if (nc < 0) die("read() from client1 failed", errno);
      printf("Read from %s returned %d bytes\n", "client1", nc);
    }
  }
  if (client2 >= 0) {
    if (!client2_connected) {
      if (FD_ISSET(client2, &writefds)) {
        client2_connected = true;
        printf("client2 connected\n");
      }
    } else if (FD_ISSET(client2, &readfds)) {
      nc = read(client2, ibuf, 80);
      if (nc < 0) die("read() from client2 failed", errno);
      printf("Read from %s returned %d bytes\n", "client2", nc);
    }
  }
  if (listener >= 0 && FD_ISSET(listener, &readfds)) {
    printf("Listener is ready for accept()\n");
    int fd = accept(listener, NULL, NULL);
    if (fd < 0) die("accept() failed after select", errno);
    printf("Client accepted\n");
    if (client1 < 0) {
      client1 = fd;
      client1_connected = true;
    } else if (client2 < 0) {
      client2 = fd;
      client2_connected = true;
    } else {
      printf("Third client connection: closing\n");
      close(fd);
    }
  }
}

void nb_app::die(const char *msg, int errorno) {
  if (errorno == 0) {
    fprintf(stderr, "%s\n", msg);
  } else {
    fprintf(stderr, "%s: %d=%s\n", msg, errorno, strerror(errorno));
  }
  exit(1);
}

void server(const char *svc) {
  nb_app srvr;
  srvr.setup_listener(svc);
  srvr.single_event(); // should be the single connect
  srvr.single_event(); // should be a read from client1
  printf("server shutting down\n");
}

void client1(const char *svc) {
  nb_app clt;
  clt.client1 = clt.client_connect(svc);
  clt.single_event();
  if (clt.client1_connected) {
    write(clt.client1, "hello", 6);
    sleep(1);
  }
  printf("client shutting down\n");
}

void client2(const char *svc) {
  nb_app clt;
  clt.client1 = clt.client_connect(svc);
  clt.client2 = clt.client_connect(svc);
  clt.single_event(); // One connection?
  clt.single_event(); // Second connection?
  if (clt.client1_connected) {
    write(clt.client1, "hello", 6);
    sleep(1);
  }
  printf("client shutting down\n");
}

int main(int argc, char **argv) {
  const char *arg = "server";
  const char *service = "service";
  if (argc > 1) arg = argv[1];
  if (strcasecmp(arg, "server") == 0) {
    server(service);
  } else if (strcasecmp(arg, "client1") == 0) {
    client1(service);
  } else if (strcasecmp(arg, "client2") == 0) {
    client2(service);
  } else {
    fprintf(stderr,"Unrecognized option\n");
    return 1;
  }
  return 0;
}
