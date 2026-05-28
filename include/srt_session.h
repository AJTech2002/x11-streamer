#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
  int sock;
  int port;
  struct sockaddr_in client;
  bool connected;
  pthread_t recvThread;
} UdpSession;

int udp_setup(UdpSession *session, int port);
int udp_send(UdpSession *session, const char *buf, size_t buf_size);
int udp_close (UdpSession *session);
void* udp_recv (void* arg);