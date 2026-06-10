#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdamage.h>
#include <stdint.h>
#include <sys/shm.h>

typedef struct {
  Display *dpy;
  Window root;
  XShmSegmentInfo shm;
  XImage *img;
  Damage damage;
  int damageEventBase;
  int width;
  int height;
} Capturer;

typedef struct {
  int x, y, w, h;
  uint8_t *pixels;
  int stride;
} DirtyFrame;

typedef void (*OnFrameCallback)(DirtyFrame *frame, void *userdata);

Capturer *capturer_create(Display *dpy, Window root, int w, int h);
void capturer_destroy(Capturer *c);
void capturer_run(Capturer *c, OnFrameCallback onFrame, void *userdata);
