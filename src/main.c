#include "xvfb_runner.h"
#include "xvfb_streamer.h"
#include "xvfb_string.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char _tempStrBuf[512];

void run_cmd(const char *str) {
  snprintf(_tempStrBuf, sizeof(_tempStrBuf), "DISPLAY=%s %s &", DISPLAY_STR,
           str);
  system(_tempStrBuf);
  usleep(300000);
}

void onFrame(DirtyFrame *frame, void *userdata) {
  printf("dirty rect: %d,%d %dx%d\n", frame->x, frame->y, frame->w, frame->h);
  debug_show(frame);
}

// everything that needs to happen before capturer_run goes here
void *typingThread(void *arg) {
  XVFB *xvfb = (XVFB *)arg;

  run_cmd("openbox");
  run_cmd("xterm -title Terminal -class Terminal");
  usleep(1000000);

  printAllWindowsByClass(xvfb->root);
  Window fWin = findWindowByClass(xvfb->root, "Terminal");
  focusWindow(fWin);
  usleep(500000);

  // type repeatedly so you can see dirty rects firing
  while (1) {
    typeString(xvfb->dpy, "echo hello world", 1);
    usleep(1000000); // 1 second between each line
  }

  return NULL;
}

static int xErrorHandler(Display *dpy, XErrorEvent *err) {
  if (err->error_code == BadWindow)
    return 0; // ignore stale windows
  char msg[256];
  XGetErrorText(dpy, err->error_code, msg, sizeof(msg));
  fprintf(stderr, "X Error: %s\n", msg);
  return 0;
}

int main() {
  printf("Starting...\n");
  XInitThreads(); // must call before any Xlib — enables thread safety
  XSetErrorHandler(xErrorHandler);
  XInitThreads();

  XVFB *xvfb = init();
  usleep(500000);

  Capturer *c = capturer_create(xvfb->dpy, xvfb->root, 1280, 720);
  debug_create(1280,720);
  // launch typing on background thread
  pthread_t thread;
  pthread_create(&thread, NULL, typingThread, xvfb);

  // blocks forever on main thread — prints dirty rect on every keystroke
  capturer_run(c, onFrame, NULL);

  capturer_destroy(c);
  debug_destroy();
  end();
  return 0;
}