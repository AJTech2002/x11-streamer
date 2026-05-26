#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
  int sock;
  struct sockaddr_in client;
  bool connected;
} UdpSession;

int udp_setup(UdpSession *session, int port);
int udp_send(UdpSession *session, const char *buf, size_t buf_size);
