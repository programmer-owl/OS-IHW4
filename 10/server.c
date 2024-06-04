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
#define MAXPENDING 7
#define BEEHIVE_SIZE 30
#define MAX_MONITOR_CLIENTS 10 // Maximum number of monitor clients
const int HALF_BEEHIVE = BEEHIVE_SIZE / 2;
int n = 0;
int sock;                    /* Socket */
struct sockaddr_in servAddr; /* Local address */
struct sockaddr_in clntAddr; /* Адрес последнего отправителя */
struct sockaddr_in bearAddr;
struct sockaddr_in hiveAddr; /* Адреса, чтобы понимать, что от кого приходит */
struct sockaddr_in clntHndlrAddr;
int pid = 0;
unsigned int bearAddrLen, hiveAddrLen, clntHndlrAddrLen,
    clntAddrLen;         /* Length of incoming message */
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

void removeMonitorClient(char *mes) {
  char strId[strlen(mes) - 2];
  for (int i = 2; i < strlen(mes); i++) {
    strId[i - 2] = mes[i];
  }
  int id = atoi(strId);
  if (id < 0 || id >= numMonitorClients) // не нашли
  {
    printf("Invalid index to remove monitor client.\n");
    return;
  }
  // Shift elements to remove the client at the given index
  for (int i = id; i < numMonitorClients - 1; i++) {
    monitorSockets[i] = monitorSockets[i + 1];
  }
  numMonitorClients--;
}

void notifyMonitor(char *message) {
  for (int i = 0; i < numMonitorClients; i++) {
    struct sockaddr_in addr = monitorSockets[i].address;
    int socket = monitorSockets[i].socket;
    if (sendto(socket, message, strlen(message), 0, (struct sockaddr *)&addr,
               sizeof(addr)) != strlen(message)) {
      DieWithError("Error sending notification to monitor client");
    }
  }
}

typedef struct {
  int bees;
  int honey_portions;
} HiveData;

HiveData *hive_data;

// имя области разделяемой памяти
const char *shar_object = "/posix-shar-object-1";

// Функция, осуществляющая при запуске манипуляции с памятью и семафорами
void my_init(int n) {
  int shmid = shm_open(shar_object, O_CREAT | O_RDWR, 0666);
  if (shmid == -1) {
    perror("shm_open");
    exit(-1);
  }
  // Задание размера объекта памяти
  if (ftruncate(shmid, sizeof(HiveData)) == -1) {
    perror("ftruncate: memory sizing error");
    exit(-1);
  }
  hive_data =
      mmap(0, sizeof(HiveData), PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
  if (hive_data == MAP_FAILED) {
    perror("mmap");
    exit(-1);
  }
  hive_data->honey_portions = 0;
}

// Функция, удаляющая разделяемую память
void my_unlink(void) {
  if (shm_unlink(shar_object) == -1) {
    perror("shm_unlink: shared memory");
    exit(-1);
  }
}

void sigint_handler(int signum) {
  my_unlink();
  char *mes =
      "You've stopped the program. The bees are grateful for saving them "
      "from the bear!\n";
  notifyMonitor(mes);
  notifyMonitor("0");
  if (sendto(sock, "0", 1, 0, (struct sockaddr *)&bearAddr,
             sizeof(bearAddr)) != 1)
    DieWithError("sendto() sent a different number of bytes than expected");
  if (sendto(sock, "0", 1, 0, (struct sockaddr *)&hiveAddr,
             sizeof(hiveAddr)) != 1)
    DieWithError("sendto() sent a different number of bytes than expected");
  close(sock); /* Close client socket */
  exit(signum);
}

void HandleBearClient() {
  if (recvMsgSize == 0)
    return;
  char *mes = "Received question from bear.\n";
  notifyMonitor(mes);
  char buffer[RCVBUFSIZE];

  if (hive_data->honey_portions < HALF_BEEHIVE)
    sprintf(buffer,
            "There are %d portions of honey in the hive. Winnie is not "
            "interested!\n",
            hive_data->honey_portions);
  else if (hive_data->bees >= 3)
    sprintf(buffer, "Ow! There are %d bees in the hive. Winnie got stung!\n",
            hive_data->bees);
  else {
    sprintf(buffer, "Success! Winnie stole the honey!\n");
    if (sendto(sock, "1", 1, 0, (struct sockaddr *)&hiveAddr,
               sizeof(hiveAddr)) != 1)
      DieWithError("sendto() sent a different number of bytes than expected");
    // сообщаем улью, что мед украли
  }
  if (sendto(sock, buffer, RCVBUFSIZE, 0, (struct sockaddr *)&bearAddr,
             sizeof(bearAddr)) != RCVBUFSIZE)
    DieWithError("sendto() sent a different number of bytes than expected");
  notifyMonitor(buffer);
  memset(buffer, 0, RCVBUFSIZE); // clean buffer
}

void HandleBeehiveClient(char *buffer) {
  if (recvMsgSize == 0)
    return;
  char mes[RCVBUFSIZE * 2];
  char new_bih[RCVBUFSIZE];
  char new_hp[RCVBUFSIZE];
  int flag = 0;
  int j1 = 0, j2 = 0;
  for (int i = 1; i < recvMsgSize - 1; i++) {
    if (j1 >= RCVBUFSIZE || j2 >= RCVBUFSIZE)
      break;
    if (buffer[i] == ' ')
      flag = 1;
    else if (flag == 0) {
      new_bih[j1] = buffer[i];
      j1 += 1;
    } else {
      new_hp[j2] = buffer[i];
      j2 += 1;
    }
  }
  hive_data->bees = atoi(new_bih);
  hive_data->honey_portions = atoi(new_hp);
  sprintf(mes,
          "Received info from the hive: there are %d bees and %d portions of "
          "honey there.\n",
          hive_data->bees, hive_data->honey_portions);
  notifyMonitor(mes);
}

int main(int argc, char *argv[]) {
  if (argc != 3) /* Test for correct number of arguments */
  {
    fprintf(stderr,
            "Usage:  %s <Number of bees greater than 3> <Server Port>\n",
            argv[0]);
    exit(1);
  }

  n = atoi(argv[1]);
  if (n <= 3) {
    printf("Expected a number greater than 3.\n");
    exit(-1);
  }

  signal(SIGINT, sigint_handler);
  my_init(n);

  servPort = atoi(argv[2]); /* local port */

  /* Create socket for sending/receiving datagrams */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError("socket() failed");

  /* Construct local address structure */
  memset(&servAddr, 0, sizeof(servAddr));       /* Zero out structure */
  servAddr.sin_family = AF_INET;                /* Internet address family */
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  servAddr.sin_port = htons(servPort);          /* Local port */

  char buf[RCVBUFSIZE];
  bool waitingForMonitor = false;

  /* Bind to the local address */
  if (bind(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    DieWithError("bind() failed");

  hiveAddrLen = sizeof(hiveAddr);
  // hello, bees! запоминаем адрес улья
  if ((recvMsgSize = recvfrom(sock, buf, RCVBUFSIZE, 0,
                              (struct sockaddr *)&hiveAddr, &hiveAddrLen)) < 0)
    DieWithError("recvfrom() failed");
  bearAddrLen = sizeof(bearAddr);
  // hello, bear! запоминаем адрес медведя
  if ((recvMsgSize = recvfrom(sock, buf, RCVBUFSIZE, 0,
                              (struct sockaddr *)&bearAddr, &bearAddrLen)) < 0)
    DieWithError("recvfrom() failed");
  clntHndlrAddrLen = sizeof(clntHndlrAddr);
  // hello, client handler! запоминаем адрес клиента-обработчика
  if ((recvMsgSize = recvfrom(sock, buf, RCVBUFSIZE, 0, (struct sockaddr *)&clntHndlrAddr, &clntHndlrAddrLen)) < 0)
    DieWithError("recvfrom() failed");

  // сообщаем улью, что получили его адрес
  if (sendto(sock, "1", 1, 0, (struct sockaddr *)&hiveAddr, sizeof(hiveAddr)) !=
      1)
    DieWithError("sendto() sent a different number of bytes than expected");
  // сообщаем медведю, что получили его адрес
  if (sendto(sock, "1", 1, 0, (struct sockaddr *)&bearAddr, sizeof(bearAddr)) !=
      1)
    DieWithError("sendto() sent a different number of bytes than expected");
  // сообщаем клиенту-обработчику, что получили его адрес
  if (sendto(sock, "1", 1, 0, (struct sockaddr *)&clntHndlrAddr, sizeof(clntHndlrAddr)) != 1)
    DieWithError("sendto() sent a different number of bytes than expected");

  while (1) { /* Run forever */
    /* Set the size of the in-out parameter */
    clntAddrLen = sizeof(clntAddr);
    char buffer[RCVBUFSIZE];

    /* Block until receive message from a client */
    if ((recvMsgSize = recvfrom(sock, buffer, RCVBUFSIZE, 0,
                                (struct sockaddr *)&clntAddr, &clntAddrLen)) <
        0)
      DieWithError("recvfrom() failed");
    if (buffer[0] == 'H') { // hive
      HandleBeehiveClient(buffer);
    } else if (buffer[0] == 'B') { // bear
      HandleBearClient();
    } else if (buffer[0] == 'C') { // clientHandler
      if (buffer[1] == 'a') {      // add monitor
        waitingForMonitor = true;
      } else { // delete monitor by its id
        removeMonitorClient(buffer);
      }
    } else if (buffer[0] == 'M') { // monitor
      if (waitingForMonitor) {
        addMonitorClient(clntAddr, sock);
        waitingForMonitor = false;
      } else
        notifyMonitor("Client handler didn't notify us, so no connection for "
                      "you, sorry!\n");
    } else {
      notifyMonitor("Who are you?\n");
    }
  }
  /* NOT REACHED */
}