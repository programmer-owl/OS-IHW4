#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h> /* for sockaddr_in and inet_ntoa() */
#include <netinet/in.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <unistd.h>     /* for close() */

#define RCVBUFSIZE 1000
#define BEEHIVE_SIZE 30
#define MAX_MONITOR_CLIENTS 10 // Maximum number of monitor clients
const int HALF_BEEHIVE = BEEHIVE_SIZE / 2;
int n = 0;
int sock;                    /* Socket */
struct sockaddr_in servAddr; /* Local address */
struct sockaddr_in clntAddr; /* Адрес последнего отправителя */
unsigned int clntAddrLen;         /* Length of incoming message */
unsigned short servPort; /* Server port */
int recvMsgSize;         /* Size of received message */
char *mes;

void DieWithError(char *errorMessage) {
  perror(errorMessage);
  exit(1);
}

// Struct to represent a monitor client
typedef struct MonitorClient {
  struct sockaddr_in address; // IP address and port
  int socket;                 // Socket descriptor
} MonitorClient;

MonitorClient monitorSockets[MAX_MONITOR_CLIENTS];
int numMonitorClients = 0; // Current number of monitor clients

void addMonitorClient(struct sockaddr_in address, int socket) {
  if (numMonitorClients >= MAX_MONITOR_CLIENTS)
    printf("Cannot add more monitor clients. Max limit reached.\n");

  MonitorClient newClient;
  newClient.address = address;
  newClient.socket = socket;
  monitorSockets[numMonitorClients++] = newClient;
  if (sendto(socket, "1", 1, 0, (struct sockaddr *)&address, sizeof(address)) <
      0) {
    DieWithError("Error sending notification to monitor client");
  }
}

void removeMonitorClient(int socket) {
  int index = 0;
  while (index < numMonitorClients) {
    if (monitorSockets[index].socket == socket)
      break;
    ++index;
  }
  if (index == numMonitorClients) // не нашли
  {
    printf("Invalid index to remove monitor client.\n");
    return;
  }
  // Shift elements to remove the client at the given index
  for (int i = index; i < numMonitorClients - 1; i++) {
    monitorSockets[i] = monitorSockets[i + 1];
  }
  numMonitorClients--;
}

void sigint_handler(int signum) {
  printf("Bye!\n");
  if (sendto(sock, "Md", strlen("Md"), 0, (struct sockaddr *)
          &servAddr, sizeof(servAddr)) != strlen("Md"))
      DieWithError("sendto() sent a different number of bytes than expected");
  exit(signum);
}

int main(int argc, char *argv[]) {
  unsigned short servPort;       /* Echo server port */
  char *servIP;                  /* Server IP address (dotted quad) */
  char *mes;                     /* String to send to echo server */
  char buffer[RCVBUFSIZE];       /* Buffer for echo string */
  unsigned int mesLen;           /* Length of string to echo */
  int bytesRcvd, totalBytesRcvd; /* Bytes read in single recv()
                                    and total bytes read */
  int n;                         /* Number of bees */

  struct sockaddr_in fromAddr;     /* Source address of echo */
  unsigned int fromSize;           /* In-out of address size for recvfrom() */
  int stringLen;               /* Length of string to echo */
  int respStringLen;               /* Length of received response */

  signal(SIGINT, sigint_handler);

  if ((argc < 2) || (argc > 3)) /* Test for correct number of arguments */
  {
    fprintf(stderr, "Usage: %s <Server IP> [<Echo Port>]\n", argv[0]);
    exit(1);
  }

  servIP = argv[1]; /* First arg: server IP address (dotted quad) */
  if (argc == 3)
    servPort = atoi(argv[2]); /* Use given port, if any */
  else
    servPort = 7; /* 7 is the well-known port for the echo service */

  servIP = argv[1]; /* First arg: server IP address (dotted quad) */
  if (argc == 3)
    servPort = atoi(argv[2]); /* Use given port, if any */
  else
    servPort = 7; /* 7 is the well-known port for the echo service */

   /* Create a datagram/UDP socket */
   if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
       DieWithError("socket() failed");

   /* Construct the server address structure */
   memset(&servAddr, 0, sizeof(servAddr));    /* Zero out structure */
   servAddr.sin_family = AF_INET;                 /* Internet addr family */
   servAddr.sin_addr.s_addr = inet_addr(servIP);  /* Server IP address */
   servAddr.sin_port   = htons(servPort);     /* Server port */

  mes = "C";
  mesLen = strlen(mes);
  /* Send the string to the server: поприветствуем сервер */
  if (sendto(sock, mes, mesLen, 0, (struct sockaddr *)
          &servAddr, sizeof(servAddr)) <= 0)
      DieWithError("sendto() sent a different number of bytes than expected");
  // receive confirmation
  fromSize = sizeof(fromAddr);
  if ((respStringLen = recvfrom(sock, buffer, RCVBUFSIZE - 1, 0,
      (struct sockaddr *) &fromAddr, &fromSize)) <= 0)
      DieWithError("recvfrom() failed");
  printf("Connected to server!\n");

  while (1) {
    printf("Enter command: 1. 'Ca' - notifies server that a monitor is about to be added; 2. 'Cd<id>' - notifies server that a monitor with this id is to be deleted.\n");
    char *line = NULL;
    size_t len = 0;
    ssize_t lineSize = 0;
    lineSize = getline(&line, &len, stdin);
    if (lineSize < 2 || line[0] != 'C') 
      printf("Wrong command!\n");
    else if (line[1] == 'a') {
      // Add a new monitor client
      /* Send the string to the server */
      if (sendto(sock, line, 2, 0, (struct sockaddr *)
              &servAddr, sizeof(servAddr)) <= 0)
          DieWithError("sendto() sent a different number of bytes than expected");
    } else if (line[1] == 'd') {
      // Delete a monitor client
      if (sendto(sock, line, strlen(line), 0, (struct sockaddr *)
          &servAddr, sizeof(servAddr)) <= 0)
      DieWithError("sendto() sent a different number of bytes than expected");
    }
    free(line);
  }
  close(sock);
  exit(0);
}
