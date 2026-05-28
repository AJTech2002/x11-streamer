#include "srt_session.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/pthreadtypes.h>
#include <bits/types/struct_osockaddr.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "logs.h"


typedef struct {
  UdpSession* session;
  pthread_mutex_t lock;
} UdpRecvThreadArgs;

void* udp_recv (void* arg) {
  UdpRecvThreadArgs* args = (UdpRecvThreadArgs*)arg;
  UdpSession* session = args->session;

  while (true) {
    pthread_mutex_lock(&args->lock);

    char buf[64];
    socklen_t len = sizeof(session->client);
    LOG_WARN_SBJ("socket", "[waiting for client punchtrough]");
    recvfrom(session->sock, buf, sizeof(buf), 0, (struct sockaddr *)&session->client, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &session->client.sin_addr, ip, sizeof(ip));

    const char *msg = "hello"; //TODO: make options
    sendto(session->sock, msg, strlen(msg), 0, (struct sockaddr *)&session->client,
    sizeof(session->client));

    session->connected = true;
    pthread_mutex_unlock(&args->lock);

  }

    return NULL;

};

int udp_setup(UdpSession *session, int port) {
  session->port = port;
  session->sock = socket(AF_INET, SOCK_DGRAM, 0);
  printf("Connected Socket : %d \n", session->sock);

  // if (sock < 0) {
  //   perror("Socket creation failed");
  //   exit(EXIT_FAILURE);
  // }
  //
  // int opt = 1;
  //
  // if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
  //   perror("setsockopt SO_REUSEADDR failed");
  //   exit(EXIT_FAILURE);
  // }
  //
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

  int bind_res = bind(session->sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
  if (bind_res != 0) {
    printf("Failed to bind: %d\n",
           bind_res); // ← strerror not just the code
    return bind_res;
  }

  UdpRecvThreadArgs* args = malloc(sizeof(UdpRecvThreadArgs));
  args->session = session;
  pthread_mutex_init(&args->lock, NULL);

  pthread_create(&session->recvThread, NULL, udp_recv, args);
  LOG_INFO_SBJ("socket", "created recv thread");

  return 0;
};

int udp_close (UdpSession *session) {
  pthread_kill(session->recvThread, SIGKILL);

  session->connected = false;
  if (session->sock >= 0) {
    close(session->sock);
    session->sock = -1;
  }

  return 0;
}

int udp_send(UdpSession* session, const char* msg, size_t buf_size) {
    if (!session->connected) return 0;

    ssize_t sent = sendto(session->sock, msg, buf_size, 0,
                          (struct sockaddr*)&session->client,
                          sizeof(session->client));
    if (sent < 0) {
      udp_close(session);
      udp_setup(session, session->port);
    }
    return sent;
}