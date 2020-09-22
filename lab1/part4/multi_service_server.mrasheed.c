#include <sys/socket.h> //socket and sockaddr
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <arpa/inet.h>

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

int open_pingfd(int port) {
  int pingfd, optval = 1;
  struct sockaddr_in serveraddr;

  if ((pingfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return -1;

  if (setsockopt(pingfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0)
    return -1;

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short) port);

  if (bind(pingfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0)
    return -1;

  return pingfd;
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
  int pingfd, listenfd, connfd, pport, port, clientlen, pid;
  int maxfd, nbytes;
  unsigned int randval;
  struct sockaddr_in clientaddr;
  fd_set rdfs;
  char pingbuf[MAXLINE], pingbufsend[MAXLINE];
  struct hostent *hp;
  char *bufend;

  if (argc < 3) {
    printf("Only %d input args. Need at least 2\n", argc - 1);
    exit(1);
  }

  signal(SIGCHLD, child_die_signal);

  port = atoi(argv[1]);
  listenfd = open_listenfd(port);
  pport = atoi(argv[2]);
  pingfd = open_pingfd(pport);
  maxfd = pingfd > listenfd ? pingfd : listenfd;
  maxfd += 1;

  clientlen = sizeof(clientaddr);

  while (1) {
    FD_ZERO(&rdfs);
    FD_SET(listenfd, &rdfs);
    FD_SET(pingfd, &rdfs);
    select(maxfd, &rdfs, NULL, NULL, NULL);

    if (FD_ISSET(listenfd, &rdfs)) {
      connfd = accept(listenfd, (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
      if ((pid = fork()) == 0) {
        handle_request(connfd);    
        close(connfd);
        // kill(getppid(), SIGCHLD);
        exit(0);
      }
      close(connfd);
    }
    else if (FD_ISSET(pingfd, &rdfs)) {
      nbytes = recvfrom(pingfd, pingbuf, MAXLINE - 1, 0, (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
      randval = 0;
      randval = (unsigned char) pingbuf[nbytes - 1];
      randval |= (unsigned char) pingbuf[nbytes - 2] << 8;
      randval |= (unsigned char) pingbuf[nbytes - 3] << 16;
      randval |= (unsigned char) pingbuf[nbytes - 4] << 24;
      pingbuf[nbytes - 4] = '\0';

      struct in_addr addr;
      inet_aton(pingbuf, &addr);
      hp = gethostbyaddr((const char *) &addr, sizeof(addr), AF_INET);

      bufend = stpcpy(pingbufsend, hp->h_name);
      randval++;
      bufend[3] = randval;
      bufend[2] = randval >> 8;
      bufend[1] = randval >> 16;
      bufend[0] = randval >> 24;
      bufend[4] = '\0';

      // sprintf(pingbufsend, "%s%d", hp->h_name, randval);
      // printf("%s %d\n", pingbufsend, randval);
      //strncpy(bufend, pingbuf + strlen(haddrp), 5);

      sendto(pingfd, pingbufsend, strlen(hp->h_name) + 4, 0, (struct sockaddr *) &clientaddr, (socklen_t) clientlen);
    }
  }

  exit(0);
}
