#pragma once
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

typedef void (*callback_fn)(int command, size_t data_size, char *data);

typedef struct {
  int sock;
  int port;
  struct sockaddr_in client;
  bool connected;
  pthread_t recvThread;
  callback_fn callback;
} UdpSession;

int udp_setup(UdpSession *session, int port, callback_fn callback);
int udp_send(UdpSession *session, const char *buf, size_t buf_size);
int udp_close(UdpSession *session);
