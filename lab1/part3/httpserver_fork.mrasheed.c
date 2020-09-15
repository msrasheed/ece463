#include <sys/socket.h> //socket and sockaddr
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#define LISTENQ 10
#define MAXLINE 256

int open_listenfd(int port) {
  int listenfd, optval = 1;
  struct sockaddr_in serveraddr;

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  // Eliminates "address already in use" error from bind
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0)
    return -1;

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short) port);

  if (bind(listenfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0)
    return -1;

  if (listen(listenfd, LISTENQ) < 0)
    return -1;

  return listenfd;
}

int handle_request(int connfd) {
  char buf[MAXLINE];
  char command[5];
  char filename[MAXLINE] = ".";
  int i, n, cnum;
  FILE * file;

  n = read(connfd, buf, MAXLINE - 1);
  buf[n] = '\0';

  sscanf(buf, "%s %s %d HTTP/1.0\r\n\r\n", command, filename + 1, &cnum);

  // printf("%s", buf);
  // printf("%s, %s, %d\n", command, filename, cnum);

  if (strcmp(command, "GET") != 0) {
    return -1;
  }

  file = fopen(filename, "r");

  if (errno == EACCES) {
    write(connfd, "HTTP/1.0 403 Forbidden\r\n\r\n", 26);
    errno = 0;
    return -2;
  }
  if (errno == ENOENT) {
    write(connfd, "HTTP/1.0 404 Not Found\r\n\r\n", 26);
    errno = 0;
    return -3;
  }

  write(connfd, "HTTP/1.0 200 OK\r\n\r\n", 19);

  while (n != 0) {
    n = fread(buf, 1, MAXLINE, file);
    for (i = 0; i < n; i++) {
      if (buf[i] >= 65 && buf[i] <= 90) {
        buf[i] = buf[i] - cnum;
        if (buf[i] < 65) buf[i] = (buf[i] - 65) + 90 + 1;
      }
      else if (buf[i] >= 97 && buf[i] <= 122) {
        buf[i] = buf[i] - cnum;
        if (buf[i] < 97) buf[i] = (buf[i] - 97) + 122 + 1;
      }
    }
    write(connfd, buf, n);
  }

  fclose(file);
  
  return 0;
}

void child_die_signal(int sig) {
  wait(NULL);
}

int main(int argc, char **argv) {
  int listenfd, connfd, port, clientlen, pid;
  struct sockaddr_in clientaddr;

  if (argc < 2) {
    printf("Only %d input args. Need at least 1\n", argc - 1);
    exit(1);
  }

  signal(SIGCHLD, child_die_signal);

  port = atoi(argv[1]);
  listenfd = open_listenfd(port);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = accept(listenfd, (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
    if ((pid = fork()) == 0) {
      handle_request(connfd);    
      close(connfd);
      // kill(getppid(), SIGCHLD);
      exit(0);
    }
    close(connfd);
  }

  exit(0);
}
