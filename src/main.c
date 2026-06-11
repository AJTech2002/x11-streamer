#include "capture.h"
#include "display.h"
#include "input.h"
#include "network.h"
#include "options.h"
#include "stream.h"
#include <X11/Xlib.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

static UdpSession session;

static int x_error_handler(Display *dpy, XErrorEvent *err) {
  char msg[256];
  XGetErrorText(dpy, err->error_code, msg, sizeof(msg));
  fprintf(stderr, "X Error: %s (req %d, minor %d, res 0x%lx)\n", msg,
          err->request_code, err->minor_code, err->resourceid);
  char req_name[256];
  XGetErrorDatabaseText(dpy, "XRequest", (char[]){err->request_code + '0', 0},
                        "unknown", req_name, sizeof(req_name));
  fprintf(stderr, "Request name: %s\n", req_name);
  void *bt[20];
  int n = backtrace(bt, 20);
  backtrace_symbols_fd(bt, n, STDERR_FILENO);
  return 0;
}

static int myIOErrorHandler(Display *dpy) {
  (void)dpy;
  fprintf(stderr, "XIO fatal error - display connection lost\n");
  return 0;
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
  XSetIOErrorHandler(myIOErrorHandler);

  Display *dpy = display_open();
  if (!dpy) return 1;

  display_set_resolution(CAPTURE_WIDTH, CAPTURE_HEIGHT);

  Window root = DefaultRootWindow(dpy);

  Display *cmd_dpy = XOpenDisplay(NULL);
  input_init(cmd_dpy);

  Capturer *c = capturer_create(dpy, root, CAPTURE_WIDTH, CAPTURE_HEIGHT);

  capturer_run(c, stream_on_frame, &session);

  capturer_destroy(c);
  display_close();
  XCloseDisplay(cmd_dpy);
  return 0;
}
