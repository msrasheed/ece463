#include "ne.h"
#include "router.h"
#include <pthread.h>

int udpfd, routerID;
struct sockaddr_in neaddr;
pthread_mutex_t lock;
FILE * logFile;

struct _neighbors {
  uint8_t isNbr;
  uint8_t isFailed;
  uint32_t cost;
  int lastUpdate;
};

#define CONVERGED 1
#define NOT_CONVERGED 0

struct _neighbors nbrs[MAX_ROUTERS];
int firstRTChange;
int lastRTChange;
uint8_t converged_flag;

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
/*
pthread_mutex_lock(&lock);
pthread_mutex_unlock(&lock);
*/

void * udpPollingThread(void * arg) {
  struct sockaddr_in rcvaddr;
  int rcvaddrlen, nbytes;
  struct pkt_RT_UPDATE updatePkt;
  struct timeval time;

  rcvaddrlen = sizeof(rcvaddr);

  while (1) {
    nbytes = recvfrom(udpfd, &updatePkt, sizeof(struct pkt_RT_UPDATE), 0, 
                        (struct sockaddr *) &rcvaddr, (socklen_t *) &rcvaddrlen);
    ntoh_pkt_RT_UPDATE(&updatePkt);
    gettimeofday(&time, NULL);

    pthread_mutex_lock(&lock);
    nbrs[updatePkt.sender_id].lastUpdate = time.tv_sec;
    nbrs[updatePkt.sender_id].isFailed = 0;
    if (UpdateRoutes(&updatePkt, nbrs[updatePkt.sender_id].cost, routerID)) {
      lastRTChange = time.tv_sec;
      converged_flag = NOT_CONVERGED;
      PrintRoutes(logFile, routerID);
      fflush(logFile);
    }
    pthread_mutex_unlock(&lock);
  }
}

void * timerPollingThread(void * arg) {
  int i, neaddrlen, nbytes;
  struct pkt_RT_UPDATE updatePktSend;
  struct timeval time;

  neaddrlen = sizeof(neaddr);

  while (1) {
    pthread_mutex_lock(&lock);
    ConvertTabletoPkt(&updatePktSend, routerID);  
    pthread_mutex_unlock(&lock);

    hton_pkt_RT_UPDATE(&updatePktSend);
    for (i = 0; i < MAX_ROUTERS; i++) {
      if (!nbrs[i].isNbr) continue;

      updatePktSend.dest_id = htonl(i);
      nbytes = sendto(udpfd, &updatePktSend, sizeof(struct pkt_RT_UPDATE), 0, 
                (struct sockaddr *) &neaddr, (socklen_t) neaddrlen);
    }

    gettimeofday(&time, NULL);

    pthread_mutex_lock(&lock);
    for (i = 0; i < MAX_ROUTERS; i++) {
      if (!nbrs[i].isNbr) continue;

      if (time.tv_sec - nbrs[i].lastUpdate >= FAILURE_DETECTION && !nbrs[i].isFailed) {
        nbrs[i].isFailed = 1;
        UninstallRoutesOnNbrDeath(i);     
        lastRTChange = time.tv_sec;
        converged_flag = NOT_CONVERGED;
        PrintRoutes(logFile, routerID);
        fflush(logFile);
      }
    }

    if (time.tv_sec - lastRTChange >= CONVERGE_TIMEOUT && !converged_flag) {
      converged_flag = CONVERGED;
      fprintf(logFile, "%d:Converged\n", (int) (time.tv_sec - firstRTChange));
      fflush(logFile);
    }
    pthread_mutex_unlock(&lock);

    sleep(UPDATE_INTERVAL);
  }
}

int main(int argc, char ** argv) {
  int i, neaddrlen, nbytes;
  uint32_t ridToSend;
  struct sockaddr_in rcvaddr;
  struct hostent *hp;
  // char readbuff[PACKETSIZE];
  struct pkt_INIT_RESPONSE init_response;
  pthread_t udpThread, timerThread;
  struct timeval time;
  char logname[14];

  if (argc < 5) {
    printf("Usage: ./router <router id> <ne hostname> <ne UDP port> <router UDP port>\n");
    exit(1);
  }

  for (i = 0; i < MAX_ROUTERS; i++) {
    nbrs[i].isNbr = 0;
  }

  routerID = atoi(argv[1]);
  udpfd = open_udpfd(atoi(argv[4]));
  nbytes = sprintf(logname, "router%d.log", routerID);
  logname[nbytes] = '\0';
  logFile = fopen(logname, "w");
  // logFile = stdout;

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

  nbytes = recvfrom(udpfd, &init_response, sizeof(struct pkt_INIT_RESPONSE), 0, 
                      (struct sockaddr *) &rcvaddr, (socklen_t *) &neaddrlen);

  ntoh_pkt_INIT_RESPONSE(&init_response);
  InitRoutingTbl(&init_response, routerID);
  gettimeofday(&time, NULL);
  for (i = 0; i < init_response.no_nbr; i++) {
    nbrs[init_response.nbrcost[i].nbr].isNbr = 1;
    nbrs[init_response.nbrcost[i].nbr].isFailed = 0;
    nbrs[init_response.nbrcost[i].nbr].cost = init_response.nbrcost[i].cost;
    nbrs[init_response.nbrcost[i].nbr].lastUpdate = time.tv_sec;
  }
  firstRTChange = time.tv_sec;
  lastRTChange = time.tv_sec;
  converged_flag = NOT_CONVERGED;

  PrintRoutes(logFile, routerID);
  fflush(logFile);

  pthread_mutex_init(&lock, NULL);
  if (pthread_create(&udpThread, NULL, udpPollingThread, NULL)) {
    perror("Error creating thread udpThread");
    return EXIT_FAILURE;
  }
  if (pthread_create(&timerThread, NULL, timerPollingThread, NULL)) {
    perror("Error creating thread timerThread");
    return EXIT_FAILURE;
  }
  
  pthread_join(udpThread, NULL);
  pthread_join(timerThread, NULL);
  
  exit(0);
}
