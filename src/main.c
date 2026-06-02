#include "logs.h"
#include "srt_session.h"
#include "xvfb_runner.h"
#include "xvfb_streamer.h"
#include "xvfb_string.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <lz4.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char _tempStrBuf[512];
UdpSession session;

void run_cmd(const char *str) {
  snprintf(_tempStrBuf, sizeof(_tempStrBuf), "DISPLAY=%s %s &", DISPLAY_STR,
           str);
  system(_tempStrBuf);
  usleep(300000);
}

void onFrame(DirtyFrame *frame, void *userdata) {
  if (!session.connected)
    return;
  debug_show(frame);

  const int pixelBytes = frame->h * frame->w * 4;
  const uint headerSize = sizeof(int) * 5;

  // separate buffers for src pixels and compressed output
  uint8_t *pixels = malloc(pixelBytes);
  uint8_t *data = malloc(headerSize + LZ4_COMPRESSBOUND(pixelBytes));
  if (!pixels || !data) {
    free(pixels);
    free(data);
    return;
  }

  // strip stride into pixels buffer
  for (int r = 0; r < frame->h; r++) {
    memcpy(pixels + r * frame->w * 4, frame->pixels + r * frame->stride,
           frame->w * 4);
  }

  int compSize =
      LZ4_compress_fast((char *)pixels, (char *)(data + headerSize), pixelBytes,
                        LZ4_COMPRESSBOUND(pixelBytes), 1);
  free(pixels);
  if (compSize <= 0) {
    free(data);
    return;
  }

  int header[5] = {frame->w, frame->h, frame->x, frame->y, pixelBytes};
  memcpy(data, header, headerSize);

  // printf("Sent Packet { w %d h %d x %d y %d uncompressed %d compressed %d
  // }\n",
  //       frame->w, frame->h, frame->x, frame->y,  pixelBytes, compSize);

  udp_send(&session, (char *)data, headerSize + compSize);
  free(data);
}

void *typingThread(void *arg) {
  XVFB *xvfb = (XVFB *)arg;

  run_cmd("openbox");
  run_cmd("xterm -title Terminal -class Terminal");
  run_cmd("xterm -title Terminal -class Terminal");

  run_cmd("xterm -title Terminal -class Terminal");

  usleep(1000000);

  printAllWindowsByClass(xvfb->root);
  Window fWin = findWindowByClass(xvfb->root, "Terminal");
  focusWindow(fWin);
  usleep(500000);

  while (1) {
    typeString(xvfb->dpy, "echo ", 0);
    usleep(1000000); // 1 second between each line

    typeString(xvfb->dpy, "\"hello ", 0);
    usleep(1000000); // 1 second between each line

    typeString(xvfb->dpy, "riti!\"", 1);

    usleep(1000000); // 1 second between each line
  }

  return NULL;
}

static int xErrorHandler(Display *dpy, XErrorEvent *err) {
  if (err->error_code == BadWindow)
    return 0;
  char msg[256];
  XGetErrorText(dpy, err->error_code, msg, sizeof(msg));
  fprintf(stderr, "X Error: %s\n", msg);
  return 0;
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    printf("No port number provided!");
    return 1;
  }

  int port = atoi(argv[1]);

  printf("Starting on port %d\n", port);
  // system("fuser -k 5201/udp");
  // system("iptables -A INPUT -p udp --dport 5201 -j ACCEPT");
  udp_setup(&session, port);

  XInitThreads();
  XSetErrorHandler(xErrorHandler);
  XInitThreads();

  XVFB *xvfb = xvfb_init();
  if (!xvfb) {
    fprintf(stderr, "Failed to initialize Xvfb\n");
    return 1;
  }
  usleep(500000);

  Capturer *c = capturer_create(xvfb->dpy, xvfb->root, 1280, 720);
  // debug_create(1280, 720);
  pthread_t thread;
  pthread_create(&thread, NULL, typingThread, xvfb);

  capturer_run(c, onFrame, NULL);

  capturer_destroy(c);
  // debug_destroy();
  xvfb_end();
  return 0;
}
