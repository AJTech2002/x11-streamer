#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "xvfb_runner.h"

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
  
  XVFB* xvfb = init();

  run_cmd("openbox");
  run_cmd("xterm");
  usleep(1000000); // wait for apps to render

  screenshot("screenshot.ppm");
  return 0;
}
