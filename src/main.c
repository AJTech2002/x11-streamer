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
  if (!session.connected) return;
  debug_show(frame);

  // TODO: Accumulate & Merge Rects & Compress
  const uint headerSize = sizeof(uint32_t) * 6;                    // 20 bytes
  const uint pixelBytes = frame->h * frame->stride;         // full rows with padding

  char* data = malloc(headerSize + LZ4_COMPRESSBOUND(pixelBytes));
  if (!data) return;

  int compSize = LZ4_compress_fast((char *)(frame->pixels), (data+headerSize),
                                   pixelBytes, LZ4_COMPRESSBOUND(pixelBytes), 1);
  if (compSize <= 0) { free(data); return; }
  printf("\rSending %d", compSize);

  const uint32_t header[6] = {
      frame->x, frame->y, frame->w, frame->h, frame->stride, pixelBytes
  }; // Send Original Size needed for decompression

  memcpy(data, header, headerSize);
  udp_send(&session, data, compSize+headerSize);
  
  free(data);
}

void *typingThread(void *arg) {
  XVFB *xvfb = (XVFB *)arg;

  run_cmd("openbox");
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
  usleep(500000);

  Capturer *c = capturer_create(xvfb->dpy, xvfb->root, 1280, 720);
  debug_create(1280, 720);
  pthread_t thread;
  pthread_create(&thread, NULL, typingThread, xvfb);

  capturer_run(c, onFrame, NULL);

  capturer_destroy(c);
  debug_destroy();
  xvfb_end();
  return 0;
}
