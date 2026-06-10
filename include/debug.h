#pragma once
#include "capture.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>

typedef struct {
  Display *dpy;
  Window win;
  GC gc;
  XImage *img;
  XShmSegmentInfo shm;
  int w, h;
} DebugWindow;

void debug_create(int w, int h);
void debug_show(DirtyFrame *frame);
void debug_destroy(void);
