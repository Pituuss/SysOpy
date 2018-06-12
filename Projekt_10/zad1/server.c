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

client clients[l_client_queue];

int instruction_counter = 0;
int client_count = 0;
int epoll;
int w_socket_desc = -1;
int l_socket_desc = -1;
char unix_path[UNIX_PATH_MAX];

pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t ping_thread;
pthread_t server_thread;

int get_client_index(char name[l_client_name]) {
  int index = -1;
  for (int i = 0; i < client_count; i++)
    if (strcmp(name, clients[i].name) == 0) {
      index = i;
      break;
    }
  return index;
}

int check_client_unique(char client_name[l_client_name]) {
  int exists = 0;
  for (int i = 0; i < client_count && exists != 1; i++) {
    if (strcmp(clients[i].name, client_name) == 0) exists = 1;
  }
  return exists;
}

void remove_socket(int socket_desc) {
  epoll_ctl(epoll, EPOLL_CTL_DEL, socket_desc, NULL);
  if (shutdown(socket_desc, SHUT_RDWR) == -1) {
    perror("failed to shutdown socket");
    exit(1);
  }
  if (close(socket_desc) == -1) {
    perror("failed to close socket");
    exit(1);
  }
}

void remove_client(int index) {
  remove_socket(clients[index].socket_desc);
  client_count -= 1;
  for (int i = index; i < client_count; i++) clients[i] = clients[i + 1];
}

void delete_client(char client_name[l_client_name]) {
  pthread_mutex_lock(&connection_mutex);
  int index = get_client_index(client_name);
  if (index != -1) {
    remove_client(index);
  }
  pthread_mutex_unlock(&connection_mutex);
}

int add_client(char client_name[l_client_name], int socket_desc) {
  pthread_mutex_lock(&connection_mutex);
  if (check_client_unique(client_name)) {
    fprintf(stderr, "client already registered\n");
    pthread_mutex_unlock(&connection_mutex);
    return 1;
  }
  if (client_count == l_client_queue) {
    fprintf(stderr, "cannot register more clients\n");
    pthread_mutex_unlock(&connection_mutex);
    return 1;
  }

  clients[client_count].socket_desc = socket_desc;
  clients[client_count].state = 1;
  strcpy(clients[client_count].name, client_name);
  client_count += 1;
  pthread_mutex_unlock(&connection_mutex);
  return 0;
}

void clean_up() {
  pthread_cancel(server_thread);
  pthread_cancel(ping_thread);
  close(w_socket_desc);
  close(l_socket_desc);
  unlink(unix_path);
  close(epoll);
  exit(EXIT_SUCCESS);
}

void init(char* s_port, char* path) {
  signal(SIGINT, clean_up);
  atexit(clean_up);

  int port = strtol(s_port, NULL, 10);
  strcpy(unix_path, path);

  //  WEB
  struct sockaddr_in w_socket_props;
  w_socket_props.sin_family = AF_INET;
  w_socket_props.sin_addr.s_addr = INADDR_ANY;
  w_socket_props.sin_port = htons(port);
  w_socket_desc = socket(AF_INET, SOCK_STREAM, 0);

  if (w_socket_desc == -1) {
    perror("failed to create listening socket web\nreason: ");
    exit(1);
  }

  if (bind(w_socket_desc, (struct sockaddr*)&w_socket_props,
           sizeof(w_socket_props)) < 0) {
    perror("failed to bind listening socket web\n");
    exit(1);
  }

  if (listen(w_socket_desc, l_client_queue) < 0) {
    perror("failed to listen\n");
    exit(1);
  }

  // LOCAL
  struct sockaddr_un l_socket_props;

  memset(&l_socket_props, '0', sizeof(l_socket_props));
  l_socket_props.sun_family = AF_UNIX;
  strcpy(l_socket_props.sun_path, unix_path);

  if ((l_socket_desc = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("failed to create listening socket local\nreason: ");
    exit(1);
  }

  if (bind(l_socket_desc, (const struct sockaddr*)&l_socket_props,
           sizeof(l_socket_props)) < 0) {
    perror("failed to create listening socket local\nreason: ");
    exit(1);
  }

  if (listen(l_socket_desc, l_client_queue) < 0) {
    perror("failed to create listening socket local\nreason: ");
    exit(1);
  }

  struct epoll_event event;

  event.events = EPOLLIN | EPOLLPRI;

  if ((epoll = epoll_create1(0)) < 0) {
    perror("failed to create epol\n");
    exit(1);
  }
  event.data.fd = -w_socket_desc;

  if (epoll_ctl(epoll, EPOLL_CTL_ADD, w_socket_desc, &event) < 0) {
    perror("failed to create epoll");
    exit(1);
  }
  event.data.fd = -l_socket_desc;

  if (epoll_ctl(epoll, EPOLL_CTL_ADD, l_socket_desc, &event) < 0) {
    perror("failed to create epoll");
    exit(1);
  }
}

void connection_handler(int socket_desc) {
  int client = accept(socket_desc, NULL, NULL);
  if (client == -1) {
    perror("client connection failed\n");
    exit(1);
  }
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLPRI;
  event.data.fd = client;

  if (epoll_ctl(epoll, EPOLL_CTL_ADD, client, &event) == -1) {
    perror("failed to create epoll\n");
    exit(1);
  }
}

void report_failure_to_client(int socket_desc) {
  char request_type = FAILURE;
  if ((write(socket_desc, &request_type, 1)) != 1) {
    fprintf(stderr, "failed to send message\n");
    exit(2);
  }
}

void report_succes_to_client(int socket_desc) {
  char request_type = SUCCES;
  if ((write(socket_desc, &request_type, 1)) != 1) {
    fprintf(stderr, "failed to send message\n");
    exit(2);
  }
}

void recieve_message_handler(int socket_desc) {
  char buff[l_client_name];
  answer ans;
  char request = -1;

  if (recv(socket_desc, &request, 1, 0) == -1) {
    perror("failed to get request type");
    // fprintf(stderr, "failed to get request type\n");
    exit(2);
  }
  switch (request) {
    case ADD_CLIENT:

      if (recv(socket_desc, buff, l_client_name, 0) == 0) {
        perror("failed to recv form socket\n");
      }
      if (add_client(buff, socket_desc) > 0) {
        report_failure_to_client(socket_desc);
      } else {
        report_succes_to_client(socket_desc);
      }
      break;

    case R_ANSWER:
      if (recv(socket_desc, &ans, sizeof(answer), 0) == 0) {
        perror("failed to recv form socket\n");
      }
      fprintf(stderr, "%d:\n result: %lf\n", ans.counter, ans.result);
      break;

    case DELETE_CLIENT:
      if (recv(socket_desc, buff, l_client_name, 0) == 0) {
        perror("failed to recv form socket\n");
      }
      delete_client(buff);
      break;

    case PONG_RQS:
      if (recv(socket_desc, buff, l_client_name, 0) == 0) {
        perror("failed to recv form socket\n");
      }
      int index = get_client_index(buff);
      if (index != -1) clients[index].state = 1;
      break;
  }
}

void* reciever(void* context) {
  struct epoll_event event;
  while (1) {
    if (epoll_wait(epoll, &event, 1, -1) == -1)  // 1-event, -1 - no timeout
    {
      perror("failed to wait for the events\n");
      exit(1);
    }
    if (event.data.fd < 0) {
      connection_handler(-event.data.fd);
    } else {
      recieve_message_handler(event.data.fd);
    }
  }
}

void* pinger(void* context) {
  char request = PING_RQS;
  while (1) {
    pthread_mutex_lock(&connection_mutex);
    for (int i = 0; i < client_count; i++) {
      if (clients[i].state == 0) {
        printf("deleting dead client: %d\n", i);
        remove_client(i);
        i = -1;
      }
    }
    for (int i = 0; i < client_count; i++) {
      if (send(clients[i].socket_desc, &request, 1, 0) != 1) {
        perror("failed to send request");
      }
      clients[i].state = 0;
    }
    pthread_mutex_unlock(&connection_mutex);
    sleep(3);
  }
}

void request_builder() {
  printf("enter req for request or stp to stop server\n");
  char buff[256];
  calc_request req;
  int counter = 0;
  while (1) {
    fgets(buff, 256, stdin);
    if (strcmp("stp\n", buff) == 0) {
      break;
    }
    if (strcmp("req\n", buff) == 0) {
      printf("enter request: \n");
      fgets(buff, 256, stdin);
      if (sscanf(buff, "%c %lf %lf", &req.operation, &req.arg1, &req.arg2) !=
          3) {
        fprintf(stderr, "failed to parse req");
      }
      req.counter = counter;
      counter += 1;

      pthread_mutex_lock(&connection_mutex);
      printf("enter client name: \n");
      fgets(buff, 256, stdin);
      int index = get_client_index(strtok(buff, "\n"));
      int desc = clients[index].socket_desc;
      pthread_mutex_unlock(&connection_mutex);
      char request = REQUEST;
      send(desc, &request, 1, 0);
      send(desc, &req, sizeof(req), 0);
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "not enougth argumnets\nPORT_NUM UNIX_PATH requred\n");
    exit(1);
  }
  init(argv[1], argv[2]);
  printf("local: %d, web: %d\n", l_socket_desc, w_socket_desc);
  pthread_create(&server_thread, NULL, reciever, NULL);
  pthread_create(&ping_thread, NULL, pinger, NULL);

  request_builder();

  pthread_cancel(server_thread);
  pthread_cancel(ping_thread);
  return 0;
}
