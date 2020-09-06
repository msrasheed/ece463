#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAXLINE 256

int open_clientfd(char *hostname, int port) {
  int clientfd;
  struct hostent *hp;
  struct sockaddr_in serveraddr;

  // open socket and get fd
  if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  // get ip of hostname
  if ((hp = gethostbyname(hostname)) == NULL)
    return -2;

  // fill struct with connection info
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy(hp->h_addr, &serveraddr.sin_addr.s_addr, hp->h_length);
  serveraddr.sin_port = htons(port);

  // make connection 
  if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    return -3;

  return clientfd;
}

int num_newlines(char * str, int len) {
  int i;
  int num = 0;
  for (i = 0; i < len; i++) {
    if (str[i] == '\n') num++;
  }
  return num;
}

int http_get_request(int clientfd, char * path, char * npath) {
  int numbytes;
  int totbytes;
  int clen;
  char buff[MAXLINE];
  char status[4];

  sprintf(buff, "GET %s HTTP/1.0\r\n\r\n", path);
  write(clientfd, buff, MAXLINE);

  buff[MAXLINE - 1] = '\0';
  status[4] = '\0';
  numbytes = read(clientfd, buff, MAXLINE - 1);
  strncpy(status, buff + 9, 3);
  printf("%s\n", status);
  printf("%s", buff);

  char * clenstr = strstr(buff, "Content-Length: ");
  clen = atoi(clenstr + 16);

  char * nnl;
  while (numbytes > 0) {
    if ((nnl = strstr(buff, "\r\n\r\n")) != NULL) break;
    numbytes = read(clientfd, buff, MAXLINE - 1);
    printf("%s", buff);
  }

  buff[nnl - buff] = '\0';
  printf("%s\n", buff);

  if (!strcmp(status, "200")) {
    return -1;
  }

  return 1;
}

int main (int argc, char **argv) {
  if (argc < 4) {
    printf("%d arguments, must have at least 3\n", argc - 1);
    //return EXIT_FAILURE;
  }

  int clientfd, port;
  // int numnls = 0;
  char *host, *path;
  char npath[MAXLINE];

  host = "dtunes.ecn.purdue.edu";// argv[1];
  port = 80; //atoi(argv[2]);
  path = "/ece463/lab1/path_short.txt"; //argv[3];

  clientfd = open_clientfd(host, port);

  if (clientfd < 0) {
    printf("Error opening connection \n");
    exit(0);
  }

  if(http_get_request(clientfd, path, npath) < 0) {
    printf("Request failed\n");
    exit(0);
  }

}
