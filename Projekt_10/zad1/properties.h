#ifndef __PROPERTIES_H_
#define __PROPERTIES_H_

#define UNIX_PATH_MAX 108

#define l_client_name 255
#define l_client_queue 16

typedef struct client {
  char name[l_client_name];
  int socket_desc;
  unsigned char state;
} client;

typedef struct {
  int counter;
  char operation;
  double arg1;
  double arg2;
} calc_request;

typedef struct {
  int counter;
  double result;
} answer;

typedef enum {
  ADD_CLIENT,
  DELETE_CLIENT,
  REQUEST,
  R_ANSWER,
  FAILURE,
  SUCCES,
  PING_RQS,
  PONG_RQS
} request_type;

typedef enum { LOCAL, WEB,BAD_MODE} client_mode;

#endif  // __PROPERTIES_H_
