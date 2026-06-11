#include "network.h"
#include "logs.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
  UdpSession *session;
  volatile bool stop;
} UdpRecvThreadArgs;

typedef struct {
  uint32_t command_id;
  uint32_t size;
  char data[];
} UdpCommand;

#define COMMAND_PUNCHTHROUGH 1

static void *udp_recv(void *arg) {
  UdpRecvThreadArgs *args = (UdpRecvThreadArgs *)arg;
  UdpSession *session = args->session;

  while (!args->stop) {
    uint32_t header[2];
    socklen_t len = sizeof(session->client);

    ssize_t r = recvfrom(session->sock, header, sizeof(header), MSG_PEEK,
                         (struct sockaddr *)&session->client, &len);
    if (r < 0) {
      if (args->stop) break;
      continue;
    }
    if (header[0] == 0) continue;

    size_t packet_size = header[1] + sizeof(header);
    char *buf = malloc(packet_size);
    if (!buf) continue;

    ssize_t r2 = recvfrom(session->sock, buf, packet_size, 0,
                          (struct sockaddr *)&session->client, &len);
    if (r2 < 0) {
      free(buf);
      if (args->stop) break;
      continue;
    }

    if (header[0] == (uint32_t)COMMAND_PUNCHTHROUGH) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &session->client.sin_addr, ip, sizeof(ip));
      LOG_OK_SBJ("socket", "client connected: %s", ip);

      const char *msg = "hello";
      sendto(session->sock, msg, strlen(msg), 0,
             (struct sockaddr *)&session->client, sizeof(session->client));

      session->connected = true;
    } else {
      UdpCommand *cmd = (UdpCommand *)buf;
      session->callback(cmd->command_id, cmd->size, cmd->data);
    }

    free(buf);
  }

  free(args);
  return NULL;
}

int udp_setup(UdpSession *session, int port, callback_fn callback) {
  session->port = port;
  session->callback = callback;
  session->sock = socket(AF_INET, SOCK_DGRAM, 0);
  printf("Socket: %d\n", session->sock);

  if (session->sock < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  if (setsockopt(session->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEADDR failed");
    exit(EXIT_FAILURE);
  }

  int tos = 0x10;
  setsockopt(session->sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));

  int sndbuf = 4 * 1024 * 1024;
  setsockopt(session->sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

  struct sockaddr_in bind_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {INADDR_ANY},
  };

  int bind_res = bind(session->sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
  if (bind_res != 0) {
    printf("Failed to bind: %d\n", bind_res);
    return bind_res;
  }

  UdpRecvThreadArgs *args = malloc(sizeof(UdpRecvThreadArgs));
  args->session = session;
  args->stop = false;

  session->recvArgs = args;
  pthread_create(&session->recvThread, NULL, udp_recv, args);
  LOG_INFO_SBJ("socket", "created recv thread");

  return 0;
}

int udp_close(UdpSession *session) {
  session->connected = false;

  if (session->recvArgs) {
    ((UdpRecvThreadArgs *)session->recvArgs)->stop = true;
    session->recvArgs = NULL; // ownership transferred to recv thread (frees itself)
  }

  if (session->sock >= 0) {
    shutdown(session->sock, SHUT_RDWR);
    close(session->sock);
    session->sock = -1;
  }

  pthread_join(session->recvThread, NULL);
  return 0;
}

int udp_send(UdpSession *session, const char *msg, size_t buf_size) {
  if (!session->connected) return 0;

  ssize_t sent = sendto(session->sock, msg, buf_size, 0,
                        (struct sockaddr *)&session->client, sizeof(session->client));
  if (sent < 0) {
    if (errno == EMSGSIZE) return 0; // frame too large for UDP, drop silently
    session->connected = false;
    LOG_INFO_SBJ("socket", "send error %d, waiting for reconnect", errno);
  }
  return (int)sent;
}
