#include "capture.h"
#include <stdio.h>
#include <stdlib.h>

Capturer *capturer_create(Display *dpy, Window root, int w, int h) {
  Capturer *c = calloc(1, sizeof(Capturer));
  c->dpy = dpy;
  c->root = root;
  c->width = w;
  c->height = h;

  int screen = DefaultScreen(dpy);
  c->img =
      XShmCreateImage(dpy, DefaultVisual(dpy, screen),
                      DefaultDepth(dpy, screen), ZPixmap, NULL, &c->shm, w, h);

  c->shm.shmid =
      shmget(IPC_PRIVATE, c->img->bytes_per_line * h, IPC_CREAT | 0777);
  c->shm.shmaddr = c->img->data = shmat(c->shm.shmid, NULL, 0);
  c->shm.readOnly = False;
  XShmAttach(dpy, &c->shm);

  int errorBase;
  XDamageQueryExtension(dpy, &c->damageEventBase, &errorBase);
  c->damage = XDamageCreate(dpy, root, XDamageReportBoundingBox);

  return c;
}

void capturer_destroy(Capturer *c) {
  if (!c)
    return;
  XDamageDestroy(c->dpy, c->damage);
  XShmDetach(c->dpy, &c->shm);
  XDestroyImage(c->img);
  shmdt(c->shm.shmaddr);
  shmctl(c->shm.shmid, IPC_RMID, NULL);
  free(c);
}

static void capturer_free_shm(Capturer *c) {
  if (!c->img)
    return;
  XShmDetach(c->dpy, &c->shm);
  XDestroyImage(c->img);
  shmdt(c->shm.shmaddr);
  shmctl(c->shm.shmid, IPC_RMID, NULL);
  c->img = NULL;
}

static void capturer_init_shm(Capturer *c) {
  int screen = DefaultScreen(c->dpy);
  c->img = XShmCreateImage(c->dpy, DefaultVisual(c->dpy, screen),
                           DefaultDepth(c->dpy, screen), ZPixmap, NULL, &c->shm,
                           c->width, c->height);
  c->shm.shmid =
      shmget(IPC_PRIVATE, c->img->bytes_per_line * c->height, IPC_CREAT | 0777);
  c->shm.shmaddr = c->img->data = shmat(c->shm.shmid, NULL, 0);
  c->shm.readOnly = False;
  XShmAttach(c->dpy, &c->shm);
}

void capturer_reinit(Capturer *c, int w, int h) {
  capturer_free_shm(c);
  c->width = w;
  c->height = h;
  capturer_init_shm(c);
  XDamageDestroy(c->dpy, c->damage);
  c->damage = XDamageCreate(c->dpy, c->root, XDamageReportBoundingBox);
  fprintf(stderr, "capturer reinit: %dx%d\n", w, h);
}

XEvent ev;

void capturer_run(Capturer *c, FragScreen *screen, OnFrameCallback onFrame,
                  void *userdata) {

  XNextEvent(c->dpy, &ev);

  // handle ConfigureNotify for root window resize
  if (ev.type == ConfigureNotify) {
    XConfigureEvent *ce = (XConfigureEvent *)&ev;
    if (ce->window == 0)
      return;
    if (ce->window == c->root &&
        (ce->width != c->width || ce->height != c->height)) {
      capturer_reinit(c, ce->width, ce->height);
    }
    return;
  }

  if (ev.type != c->damageEventBase + XDamageNotify)
    return;

  // coalesce all pending damage events into one bounding box
  XDamageNotifyEvent *dev = (XDamageNotifyEvent *)&ev;
  int x = dev->area.x, y = dev->area.y;
  int xmax = x + dev->area.width, ymax = y + dev->area.height;

  while (XPending(c->dpy)) {
    XEvent ev2;
    XPeekEvent(c->dpy, &ev2);
    if (ev2.type == c->damageEventBase + XDamageNotify) {
      XNextEvent(c->dpy, &ev2);
      XDamageNotifyEvent *d2 = (XDamageNotifyEvent *)&ev2;
      int nx = d2->area.x, ny = d2->area.y;
      int nx2 = nx + d2->area.width, ny2 = ny + d2->area.height;
      if (nx < x)
        x = nx;
      if (ny < y)
        y = ny;
      if (nx2 > xmax)
        xmax = nx2;
      if (ny2 > ymax)
        ymax = ny2;
    } else if (ev2.type == ConfigureNotify) {
      // handle resize events queued during drain
      XNextEvent(c->dpy, &ev2);
      XConfigureEvent *ce = (XConfigureEvent *)&ev2;
      if (ce->window != 0 && ce->window == c->root &&
          (ce->width != c->width || ce->height != c->height)) {
        capturer_reinit(c, ce->width, ce->height);
      }
    } else {
      break; // leave non-damage non-configure events in queue
    }
  }

  int w = xmax - x, h = ymax - y;

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x + w > c->width)
    w = c->width - x;
  if (y + h > c->height)
    h = c->height - y;
  if (w <= 0 || h <= 0)
    goto reset;

  if (!XShmGetImage(c->dpy, c->root, c->img, 0, 0, AllPlanes)) {
    fprintf(stderr, "XShmGetImage failed\n");
    goto reset;
  }

  DirtyFrame frame = {
      .x = x,
      .y = y,
      .w = w,
      .h = h,
      .pixels = (uint8_t *)c->img->data + y * c->img->bytes_per_line + x * 4,
      .stride = c->img->bytes_per_line,
  };

  onFrame(&frame, screen, userdata);

reset:
  XDamageSubtract(c->dpy, c->damage, None, None);
}