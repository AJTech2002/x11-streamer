#include "capture.h"
#include "display.h"
#include "input.h"
#include "network.h"
#include "options.h"
#include "stream.h"
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static UdpSession session;

static int x_error_handler(Display *dpy, XErrorEvent *err) {
  char msg[256];
  XGetErrorText(dpy, err->error_code, msg, sizeof(msg));
  fprintf(stderr, "X Error: %s (req %d, minor %d, res 0x%lx)\n", msg,
          err->request_code, err->minor_code, err->resourceid);
  void *bt[20];
  int n = backtrace(bt, 20);
  backtrace_symbols_fd(bt, n, STDERR_FILENO);
  return 0;
}

static void run_cmd(const char *str) {
  char buf[512];
  snprintf(buf, sizeof(buf), "DISPLAY=%s %s &", DISPLAY_STR, str);
  system(buf);
  usleep(300000);
}

static void *typing_thread(void *arg) {
  (void)arg;
  Display *dpy = XOpenDisplay(DISPLAY_STR);
  if (!dpy)
    return NULL;

  XSynchronize(dpy, True);
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

  // while (1) {
  //   typeString(dpy, "echo ", 0);
  //   usleep(1000000);
  //   typeString(dpy, "\"hello ", 0);
  //   usleep(1000000);
  //   typeString(dpy, "riti!\"", 1);
  //   usleep(5000000);
  // }

  XCloseDisplay(dpy);
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("No port number provided!\n");
    return 1;
  }

  int port = atoi(argv[1]);
  printf("Starting on port %d\n", port);

  udp_setup(&session, port, on_command);

  XInitThreads();
  XSetErrorHandler(x_error_handler);

  XVFB *xvfb = xvfb_init();
  if (!xvfb) {
    fprintf(stderr, "Failed to initialize Xvfb\n");
    return 1;
  }
  usleep(500000);

  Display *cmd_dpy = XOpenDisplay(DISPLAY_STR);
  input_init(cmd_dpy);

  Capturer *c = capturer_create(xvfb->dpy, xvfb->root, 1280, 720);

  pthread_t thread;
  pthread_create(&thread, NULL, typing_thread, xvfb);

  capturer_run(c, stream_on_frame, &session);

  capturer_destroy(c);
  xvfb_end();
  XCloseDisplay(cmd_dpy);
  return 0;
}
