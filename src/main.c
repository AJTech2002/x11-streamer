#include "logs.h"
#include "srt_session.h"
#include "xvfb_runner.h"
#include "xvfb_streamer.h"
#include "xvfb_string.h"
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <execinfo.h>
#include <lz4.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCREEN_WIDTH 1280.0
#define SCREEN_HEIGHT 720.0

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

  uint8_t *pixels = malloc(pixelBytes);
  uint8_t *data = malloc(headerSize + LZ4_COMPRESSBOUND(pixelBytes));
  if (!pixels || !data) {
    free(pixels);
    free(data);
    return;
  }

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

  udp_send(&session, (char *)data, headerSize + compSize);
  free(data);
}

// ── Typing thread — owns its own Display* connection, no locking needed ──────

void *typingThread(void *arg) {
  XVFB *xvfb = (XVFB *)arg;

  // open a separate connection to the same Xvfb
  Display *dpy = XOpenDisplay(DISPLAY_STR);
  if (!dpy) {
    fprintf(stderr, "[typingThread] Failed to open display\n");
    return NULL;
  }

  XSynchronize(dpy, True); // helps surface real errors
  // force Xkb init
  XkbDescPtr xkb = XkbGetMap(dpy, 0, XkbUseCoreKbd);
  if (xkb)
    XkbFreeKeyboard(xkb, 0, True);

  Window root = RootWindow(dpy, DefaultScreen(dpy));

  run_cmd("openbox");
  run_cmd("xterm -title Terminal -class Terminal");
  usleep(1000000);

  printAllWindowsByClass(root);
  Window fWin = findWindowByClass(root, "Terminal");
  focusWindow(fWin);
  usleep(500000);

  while (1) {
    typeString(dpy, "echo ", 0);
    usleep(1000000);

    typeString(dpy, "\"hello ", 0);
    usleep(1000000);

    typeString(dpy, "riti!\"", 1);
    usleep(5000000);
  }

  XCloseDisplay(dpy);
  return NULL;
}

// ── X error handler
// ───────────────────────────────────────────────────────────

int xErrorHandler(Display *dpy, XErrorEvent *err) {
  char msg[256];
  XGetErrorText(dpy, err->error_code, msg, sizeof(msg));
  fprintf(stderr, "X Error: %s\n", msg);
  fprintf(stderr, "  request code: %d\n", err->request_code);
  fprintf(stderr, "  minor code:   %d\n", err->minor_code);
  fprintf(stderr, "  resource id:  0x%lx\n", err->resourceid);
  void *bt[20];
  int n = backtrace(bt, 20);
  backtrace_symbols_fd(bt, n, STDERR_FILENO);
  return 0;
}

#define COMMAND_MOUSE_CLICK 2
#define COMMAND_MOUSE_MOVE 3
Display *cmd_dpy = NULL; // global
bool currently_pressed = false;

void on_command(int command, size_t data_size, char *data) {
  // LOG_OK_SBJ("commands", "received command %d with size %d", command,

  //            data_size);
  typedef struct {
    float percX;
    float percY;
    bool down;
  } ClickPayload;
  Window root = RootWindow(cmd_dpy, DefaultScreen(cmd_dpy));
  if (command == COMMAND_MOUSE_CLICK) {

    ClickPayload *payload = (ClickPayload *)data;

    int x = round(SCREEN_WIDTH * payload->percX);
    int y = round(SCREEN_HEIGHT * payload->percY);

    XWarpPointer(cmd_dpy, None, root, 0, 0, 0, 0, x, y);
    XFlush(cmd_dpy);

    LOG_INFO_SBJ("click down", "[%f, %f]", payload->percX, payload->percY);

    XTestFakeButtonEvent(cmd_dpy, 1, payload->down, CurrentTime); // press
    XFlush(cmd_dpy);
    currently_pressed = payload->down;

  } else if (command == COMMAND_MOUSE_MOVE) {
    ClickPayload *payload = (ClickPayload *)data;

    int x = round(SCREEN_WIDTH * payload->percX);
    int y = round(SCREEN_HEIGHT * payload->percY);

    // Update click state if it was missed by UDP
    if (payload->down != currently_pressed) {
      XTestFakeButtonEvent(cmd_dpy, 1, payload->down,
                           CurrentTime); // press
      XFlush(cmd_dpy);
      currently_pressed = payload->down;
    }

    XWarpPointer(cmd_dpy, None, root, 0, 0, 0, 0, x, y);
    XFlush(cmd_dpy);
  }
}

// ── main
// ──────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("No port number provided!\n");
    return 1;
  }

  int port = atoi(argv[1]);
  printf("Starting on port %d\n", port);

  udp_setup(&session, port, &on_command);

  XInitThreads();
  XSetErrorHandler(xErrorHandler);

  XVFB *xvfb = xvfb_init();
  if (!xvfb) {
    fprintf(stderr, "Failed to initialize Xvfb\n");
    return 1;
  }
  usleep(500000);
  cmd_dpy = XOpenDisplay(DISPLAY_STR);
  Window cmd_root = RootWindow(cmd_dpy, DefaultScreen(cmd_dpy));

  // capturer owns xvfb->dpy exclusively — nothing else touches it
  Capturer *c = capturer_create(xvfb->dpy, xvfb->root, 1280, 720);

  // debug window on a third connection (NULL = $DISPLAY = your Hyprland)
  // debug_create(1280, 720);

  // typing thread opens its own fourth connection inside
  pthread_t thread;
  pthread_create(&thread, NULL, typingThread, xvfb);

  capturer_run(c, onFrame, NULL); // blocks forever

  capturer_destroy(c);
  // debug_destroy();
  xvfb_end();
  XCloseDisplay(cmd_dpy);
  return 0;
}