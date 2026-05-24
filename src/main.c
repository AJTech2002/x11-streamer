#include "xvfb_runner.h"
#include "xvfb_string.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t xvfbPid = -1;
char _tempStrBuf[512];

void run_cmd(const char *str) {
  snprintf(_tempStrBuf, sizeof(_tempStrBuf), "DISPLAY=%s %s &", DISPLAY_STR,
           str);
  system(_tempStrBuf);
  usleep(300000);
}

int main() {
  printf("Starting...\n");

  XVFB *xvfb = init();
  usleep(2000000);

  run_cmd("openbox");
  run_cmd("xterm -title Terminal -class Terminal");
  usleep(1000000); // wait for apps to render
  printAllWindowsByClass(xvfb->root);

  Window fWin = findWindowByClass(xvfb->root, "Terminal");
  focusWindow(fWin);

  usleep(1e6);

  typeString(xvfb->dpy, "ls -a ~", 1);

  usleep(1e6);
  usleep(1e6);

  screenshot("screenshot.ppm");
  return 0;
}
