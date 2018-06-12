#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "properties.h"

int epoll;
int sock = 0;
char name[l_client_name];
client_mode c_mode;
char unix_path[UNIX_PATH_MAX];

void init(char* c_name, char* server_addres, char* mode) {
  strcpy(name, c_name);

  c_mode = (strcmp(mode, "web") == 0
                ? WEB
                : strcmp(mode, "local") == 0 ? LOCAL : BAD_MODE);

  if (c_mode == BAD_MODE) {
    fprintf(stderr, "bad mode\n");
    exit(1);
  } else if (c_mode == WEB) {
    char* ip_addr = strtok(server_addres, ":");
    char* port_s = strtok(NULL, ":");
    int port = strtol(port_s, NULL, 10);

    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      printf("\n Socket creation error\n");
      exit(1);
    }
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr, &serv_addr.sin_addr) <= 0) {
      printf("\nInvalid address/ Address not supported\n");
      exit(1);
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      printf("\nConnection Failed\n");
      exit(1);
    }
    fprintf(stderr, "connected at %s:%d with name \"%s\"\n", ip_addr, port,
            name);
  } else {
    strcpy(unix_path, server_addres);

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
      perror("failed to create listening socket local\nreason: ");
      exit(1);
    }
    struct sockaddr_un serv_addr;

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, unix_path);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      printf("\nConnection Failed\n");
      exit(1);
    }
    fprintf(stderr, "connected at %s with name \"%s\"\n", serv_addr.sun_path,
            name);
  }

  struct epoll_event event;

  event.events = EPOLLIN | EPOLLPRI;

  if ((epoll = epoll_create1(0)) == -1) {
    perror("failed to create epol\n");
    exit(1);
  }
  event.data.fd = sock;

  if (epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &event) == -1) {
    perror("failed to create epoll");
    exit(1);
  }
}

void recieve_message_handler() {
  char request = -1;
  if (recv(sock, &request, 1, 0) == -1) {
    fprintf(stderr, "failed to get request type\n");
    exit(2);
  }
  calc_request rqs;
  switch (request) {
    case REQUEST:
      if (recv(sock, &rqs, sizeof(calc_request), 0) == 0) {
        perror("failed to recv form socket\n");
      }
      answer ans;
      ans.counter = rqs.counter;
      switch (rqs.operation) {
        case '+':
          ans.result = rqs.arg1 + rqs.arg2;
          break;
        case '-':
          ans.result = rqs.arg1 - rqs.arg2;
          break;
        case '/':
          ans.result = rqs.arg1 / rqs.arg2;
          break;
        case '*':
          ans.result = rqs.arg1 * rqs.arg2;
          break;
      }
      request = R_ANSWER;
      send(sock, &request, 1, 0);
      send(sock, &ans, sizeof(answer), 0);
      break;
    case FAILURE:
      exit(1);
      break;
    case PING_RQS:
      request = PONG_RQS;
      send(sock, &request, 1, 0);
      send(sock, name, strlen(name), 0);
      break;
    case SUCCES:
      break;
  }
}

void reciever() {
  struct epoll_event event;
  while (1) {
    if (epoll_wait(epoll, &event, 1, -1) == -1)  // 1-event, -1 - no timeout
    {
      perror("failed to wait for the events\n");
      exit(1);
    }
    recieve_message_handler();
  }
}

void clean_up() {
  char request = DELETE_CLIENT;
  send(sock, &request, 1, 0);
  fprintf(stderr, "CLIENT OUT\n");
  send(sock, name, strlen(name), 0);
  shutdown(sock, SHUT_RDWR);
  close(sock);
}

int main(int argc, char** argv) {
  atexit(clean_up);
  signal(SIGINT, clean_up);

  init(argv[1], argv[2], argv[3]);

  printf("sock: %d\n", sock);

  char request = ADD_CLIENT;
  if (send(sock, &request, 1, 0) < 0) {
    perror("send:");
    exit(1);
  }
  send(sock, name, strlen(name), 0);

  reciever();

  return 0;
}