#include "ne.h"
#include "router.h"

int open_udpfd(int port) {
  int udpfd, optval = 1;
  struct sockaddr_in serveraddr;

  if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    return -1;

  if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0)
    return -1;

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short) port);

  if (bind(udpfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0)
    return -1;

  return udpfd;
}

int main(int argc, char ** argv) {
  int udpfd;
  int routerID, neaddrlen, nbytes;
  uint32_t ridToSend;
  struct sockaddr_in neaddr;
  struct hostent *hp;
  // char readbuff[PACKETSIZE];
  struct pkt_INIT_RESPONSE init_response;

  if (argc < 5) {
    printf("Usage: ./router <router id> <ne hostname> <ne UDP port> <router UDP port>\n");
    exit(1);
  }

  routerID = atoi(argv[1]);
  udpfd = open_udpfd(atoi(argv[4]));

  if ((hp = gethostbyname(argv[2])) == NULL)
    exit(1);
  bzero(&neaddr, sizeof(neaddr));
  neaddr.sin_family = AF_INET;
  bcopy(hp->h_addr, &neaddr.sin_addr.s_addr, hp->h_length);
  neaddr.sin_port = htons(atoi(argv[3]));

  neaddrlen = sizeof(neaddr);

  ridToSend = htonl(routerID);
  nbytes = sendto(udpfd, &ridToSend, sizeof(uint32_t), 0, (struct sockaddr *) &neaddr, 
          (socklen_t) neaddrlen);
  printf("nbytes: %d\n", nbytes);

  nbytes = recvfrom(udpfd, &init_response, sizeof(struct pkt_INIT_RESPONSE), 0, 
                      (struct sockaddr *) &neaddr, (socklen_t *) &neaddrlen);
  printf("nbytes: %d\n", nbytes);

  ntoh_pkt_INIT_RESPONSE(&init_response);
  InitRoutingTbl(&init_response, routerID);

  PrintRoutes(stdout, routerID);
  
  exit(0);
}
