#include "srt_session.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_osockaddr.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int udp_setup(UdpSession *session, int port) {
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
  printf("Bound successfully, waiting...\n");
  // wait for the iPhone to punch through
  printf(" ~~ Waiting for client...\n");
  char buf[64];
  socklen_t len = sizeof(session->client);
  recvfrom(session->sock, buf, sizeof(buf), 0, (struct sockaddr *)&session->client, &len);

  printf("Got the packet!");

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &session->client.sin_addr, ip, sizeof(ip));
  printf("Client: %s:%d — blasting\n", ip, ntohs(session->client.sin_port));
  const char *msg = "hello";
  sendto(session->sock, msg, strlen(msg), 0, (struct sockaddr *)&session->client,
  sizeof(session->client));
  session->connected = true;
  return 0;
}


int udp_send(UdpSession* session, const char *msg, size_t buf_size) {
  if (session->connected) {

    return sendto(session->sock, msg, buf_size, 0, (struct sockaddr *)&(session->client),
                  sizeof(session->client));
  } else
    return 0;
}
