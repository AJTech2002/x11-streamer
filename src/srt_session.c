#include "srt_session.h"
#include "logs.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/pthreadtypes.h>
#include <bits/types/struct_osockaddr.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
  UdpSession *session;
  pthread_mutex_t lock;
  callback_fn callback;
} UdpRecvThreadArgs;

typedef struct {
  uint32_t command_id;
  uint32_t size;
  char data[];
} UdpCommand;

#define COMMAND_PUNCHTROUGH 1

void *udp_recv(void *arg) {
  UdpRecvThreadArgs *args = (UdpRecvThreadArgs *)arg;
  UdpSession *session = args->session;

  while (true) {
    uint32_t header[2];
    socklen_t len = sizeof(session->client);
    // LOG_WARN_SBJ("socket", "listening for commands");

    ssize_t r = recvfrom(session->sock, header, sizeof(header), MSG_PEEK,
                         (struct sockaddr *)&session->client, &len);
    if (r < 0)
      continue;

    if (header[0] != 0) {

      size_t incoming_data_size = header[1] + sizeof(header);
      char *packet_buf = malloc(incoming_data_size);

      ssize_t r2 = recvfrom(session->sock, packet_buf, incoming_data_size, 0,
                            (struct sockaddr *)&session->client, &len);

      if (r2 < 0)
        continue;

      if (header[0] == (uint32_t)COMMAND_PUNCHTROUGH) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &session->client.sin_addr, ip, sizeof(ip));
        LOG_OK_SBJ("socket", "client connected: %s", ip);

        const char *msg = "hello";
        sendto(session->sock, msg, strlen(msg), 0,
               (struct sockaddr *)&session->client, sizeof(session->client));

        pthread_mutex_lock(&args->lock);
        session->connected = true;
        pthread_mutex_unlock(&args->lock);
      } else {
        // Other commands are handled in the command handler (TODO)
        UdpCommand *cmd = (UdpCommand *)packet_buf;
        args->callback(cmd->command_id, cmd->size, cmd->data);
      }

      free(packet_buf);
    }
  }
  return NULL;
}

int udp_setup(UdpSession *session, int port, callback_fn callback) {
  session->port = port;
  session->sock = socket(AF_INET, SOCK_DGRAM, 0);
  printf("Connected Socket : %d \n", session->sock);

  if (session->sock < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  int opt = 1;

  if (setsockopt(session->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    perror("setsockopt SO_REUSEADDR failed");
    exit(EXIT_FAILURE);
  }

  int tos = 0x10;
  setsockopt(session->sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));

  int sndbuf = 4 * 1024 * 1024;
  setsockopt(session->sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

  // setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, "wlan0", strlen("wlan0"));

  struct sockaddr_in bind_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {INADDR_ANY},
      // .sin_addr.s_addr = inet_addr("192.168.x.x"), // your wlan1 IP
  };

  int bind_res =
      bind(session->sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
  if (bind_res != 0) {
    printf("Failed to bind: %d\n",
           bind_res); // ← strerror not just the code
    return bind_res;
  }

  UdpRecvThreadArgs *args = malloc(sizeof(UdpRecvThreadArgs));
  args->session = session;
  args->callback = callback;
  pthread_mutex_init(&args->lock, NULL);

  pthread_create(&session->recvThread, NULL, udp_recv, args);
  LOG_INFO_SBJ("socket", "created recv thread");

  return 0;
};

int udp_close(UdpSession *session) {
  pthread_kill(session->recvThread, SIGKILL);

  session->connected = false;
  if (session->sock >= 0) {
    close(session->sock);
    session->sock = -1;
  }

  return 0;
}

int udp_send(UdpSession *session, const char *msg, size_t buf_size) {
  if (!session->connected)
    return 0;

  ssize_t sent =
      sendto(session->sock, msg, buf_size, 0,
             (struct sockaddr *)&session->client, sizeof(session->client));
  if (sent < 0) {
    udp_close(session);
    udp_setup(session, session->port, session->callback);
  }
  return sent;
}
