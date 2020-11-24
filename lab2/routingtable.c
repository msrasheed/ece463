#include "ne.h"
#include "router.h"

#define ROUTE_ENTRY_EMPTY 0
#define ROUTE_ENTRY_FILLED 1

/* ----- GLOBAL VARIABLES ----- */
struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes;
uint8_t route_init[MAX_ROUTERS];


////////////////////////////////////////////////////////////////
void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID){
	/* ----- YOUR CODE HERE ----- */
  int i;
  for (i = 0; i < MAX_ROUTERS; i++) {
    route_init[i] = ROUTE_ENTRY_EMPTY;
  }
  NumRoutes = 0;

  routingTable[myID].dest_id = myID;
  routingTable[myID].next_hop = myID;
  routingTable[myID].cost = 0;
  routingTable[myID].path_len = 1;
  routingTable[myID].path[0] = myID;
  route_init[myID] = ROUTE_ENTRY_FILLED;

  for (i = 0; i < InitResponse->no_nbr; i++) {
    struct nbr_cost * n = &InitResponse->nbrcost[i];
    routingTable[n->nbr].dest_id = n->nbr;
    routingTable[n->nbr].next_hop = n->nbr;
    routingTable[n->nbr].cost = n->cost;
    routingTable[n->nbr].path_len = 2;
    routingTable[n->nbr].path[0] = myID;
    routingTable[n->nbr].path[1] = n->nbr;
    route_init[n->nbr] = ROUTE_ENTRY_FILLED;
  }

  NumRoutes = InitResponse->no_nbr + 1;
	return;
}

int idNotInRoute(struct route_entry * re, unsigned int id) {
  int i;
  for (i = 0; i < re->path_len; i++) {
    // printf("%d ", re->path[i]);
    if (re->path[i] == id) {
      // printf("\n");
      return 0;
    }
  }
  // printf("\n");
  return 1;
}

void copyPath(struct route_entry * dest, struct route_entry * src, int myID) {
  int j;
  dest->path_len = src->path_len + 1;
  dest->path[0] = myID;
  for (j = 0; j < src->path_len; j++) {
    dest->path[j + 1] = src->path[j];
  }
}

int isNbrUpdateNecessary(struct route_entry * inre, int in_cost) {
  int i;
  struct route_entry * stored = &routingTable[inre->dest_id];
  // printf("%d %d\n", stored->cost, in_cost);
  if (stored->cost != in_cost) return 1;
  if (stored->path_len != inre->path_len + 1) return 1;
  for (i = 1; i < stored->path_len; i++) {
    if (stored->path[i] != inre->path[i - 1]) return 1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID){
	/* ----- YOUR CODE HERE ----- */
  int i;
  int numflag = RecvdUpdatePacket->no_routes;
  for (i = 0; i < RecvdUpdatePacket->no_routes; i++) {
    struct route_entry * re = &RecvdUpdatePacket->route[i];
    unsigned int ncost = re->cost + costToNbr;
    if (ncost > INFINITY) ncost = INFINITY;

    if (route_init[re->dest_id] == ROUTE_ENTRY_EMPTY) {
      routingTable[re->dest_id].dest_id = re->dest_id;
      routingTable[re->dest_id].next_hop = RecvdUpdatePacket->sender_id;
      routingTable[re->dest_id].cost = ncost;
      copyPath(&routingTable[re->dest_id], re, myID);
      route_init[re->dest_id] = ROUTE_ENTRY_FILLED;
      NumRoutes++;
    }
    else if ((RecvdUpdatePacket->sender_id == routingTable[re->dest_id].next_hop && 
                isNbrUpdateNecessary(re, ncost)) || 
                (ncost < routingTable[re->dest_id].cost && idNotInRoute(re, myID))) {
      routingTable[re->dest_id].next_hop = RecvdUpdatePacket->sender_id;
      routingTable[re->dest_id].cost = ncost;
      copyPath(&routingTable[re->dest_id], re, myID);
    }
    // else if (ncost < routingTable[re->dest_id].cost && idNotInRoute(re, myID)) {
    //   routingTable[re->dest_id].next_hop = RecvdUpdatePacket->sender_id;
    //   routingTable[re->dest_id].cost = ncost;
    //   copyPath(&routingTable[re->dest_id], re, myID);
    // }
    else {
      numflag--;
    }
  }
	return numflag > 0;
}


////////////////////////////////////////////////////////////////
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID){
	/* ----- YOUR CODE HERE ----- */
  UpdatePacketToSend->sender_id = myID;
  UpdatePacketToSend->no_routes = NumRoutes;

  int i, j=0;
  for (i = 0; i < MAX_ROUTERS; i++) {
    if (route_init[i] == ROUTE_ENTRY_FILLED) {
      memcpy(&UpdatePacketToSend->route[j], routingTable + i, sizeof(struct route_entry));
      j++;
    }
  }
	return;
}


////////////////////////////////////////////////////////////////
//It is highly recommended that you do not change this function!
void PrintRoutes (FILE* Logfile, int myID){
	/* ----- PRINT ALL ROUTES TO LOG FILE ----- */
	int i;
	int j;
	for(i = 0; i < MAX_ROUTERS; i++){
    if (route_init[i] == ROUTE_ENTRY_EMPTY) {
      continue;
    }
		fprintf(Logfile, "<R%d -> R%d> Path: R%d", myID, routingTable[i].dest_id, myID);

		/* ----- PRINT PATH VECTOR ----- */
		for(j = 1; j < routingTable[i].path_len; j++){
			fprintf(Logfile, " -> R%d", routingTable[i].path[j]);	
		}
		fprintf(Logfile, ", Cost: %d\n", routingTable[i].cost);
	}
	fprintf(Logfile, "\n");
	fflush(Logfile);
}


////////////////////////////////////////////////////////////////
void UninstallRoutesOnNbrDeath(int DeadNbr){
	/* ----- YOUR CODE HERE ----- */
  int i;
  for (i = 0; i < MAX_ROUTERS; i++) {
    if (routingTable[i].next_hop == DeadNbr) {
      routingTable[i].cost = INFINITY;
    }
  }
	return;
}
