#include <signal.h>
#include <time.h>
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <unistd.h>     /* for close() */

#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <unistd.h>     /* for close() */

#define RCVBUFSIZE 2000
int sock; /* Socket descriptor */

void DieWithError(char *errorMessage) {
  perror(errorMessage);
  exit(1);
}

void sigint_handler(int signum) {
  printf("Bye!\n");
  exit(signum);
}

int main(int argc, char *argv[]) {
  struct sockaddr_in servAddr;   /* Echo server address */
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

  mes = "M";
  mesLen = strlen(mes);
  /* Send the string to the server: поприветствуем сервер */
  if (sendto(sock, mes, mesLen, 0, (struct sockaddr *)
          &servAddr, sizeof(servAddr)) != mesLen)
      DieWithError("sendto() sent a different number of bytes than expected");
  // receive confirmation
  fromSize = sizeof(fromAddr);
  if ((respStringLen = recvfrom(sock, buffer, RCVBUFSIZE - 1, 0,
      (struct sockaddr *) &fromAddr, &fromSize)) <= 0)
      DieWithError("recvfrom() failed");
  printf("Connected to server!\n");

  while (1) {
    fromSize = sizeof(fromAddr);
    if ((respStringLen = recvfrom(sock, buffer, RCVBUFSIZE, 0,
        (struct sockaddr *) &fromAddr, &fromSize)) <= 0)
        DieWithError("recvfrom() failed");

    if (servAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr) {
        DieWithError("Error: received a packet from unknown source.\n");
    }
    if (buffer[0] == '0') 
      sigint_handler(1);
    // print the answer from server
    printf("%s", buffer);
  }
  close(sock);
  exit(0);
}
