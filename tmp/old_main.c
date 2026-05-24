#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define DISPLAY_IDX 90
#define DISPLAY_STR ":90"

pid_t xvfbPid = -1;
char _tempStrBuf[512];

void stopXvfb() {
  if (xvfbPid > 0) {
    kill(xvfbPid, SIGTERM);
    waitpid(xvfbPid, NULL, 0);
    xvfbPid = -1;
  } else {
    system("rm -f /tmp/.X90-lock");
    system("pkill Xvfb 2>/dev/null");
    usleep(200000);
  }
}

void startXvfb() {
  stopXvfb();

  xvfbPid = fork();
  if (xvfbPid == 0) {
    execlp("Xvfb", "Xvfb", DISPLAY_STR, "-screen", "0", "1280x720x24", "-ac",
           "+extension", "GLX", NULL);
    _exit(1);
  }

  // retry until display is ready
  Display *dpy = NULL;
  for (int i = 0; i < 30; i++) {
    usleep(200000);
    dpy = XOpenDisplay(DISPLAY_STR);
    if (dpy) {
      XCloseDisplay(dpy);
      printf("Xvfb ready on %s\n", DISPLAY_STR);
      return;
    }
  }
  fprintf(stderr, "Xvfb failed to start\n");
  exit(1);
}

void run_cmd(const char *str) {
  // set DISPLAY and run in background
  snprintf(_tempStrBuf, sizeof(_tempStrBuf), "DISPLAY=%s %s &", DISPLAY_STR,
           str);
  system(_tempStrBuf);
  usleep(200000); // give it time to launch
}

int main() {
  printf("Starting...\n");

  startXvfb();
  run_cmd("openbox");
  run_cmd("xterm");

  // connect
  Display *dpy = XOpenDisplay(DISPLAY_STR);
  if (!dpy) {
    fprintf(stderr, "Failed to open display\n");
    stopXvfb();
    return 1;
  }

  printf("Connected to display\n");
  Window root = DefaultRootWindow(dpy);

  // do your X11 work here

  system("read");
  printf("Clsoing display\n");

  XCloseDisplay(dpy);
  stopXvfb();
  return 0;
}
