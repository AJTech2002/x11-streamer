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
  c->img = XShmCreateImage(dpy, DefaultVisual(dpy, screen),
                           DefaultDepth(dpy, screen), ZPixmap, NULL, &c->shm, w, h);

  c->shm.shmid = shmget(IPC_PRIVATE, c->img->bytes_per_line * h, IPC_CREAT | 0777);
  c->shm.shmaddr = c->img->data = shmat(c->shm.shmid, NULL, 0);
  c->shm.readOnly = False;
  XShmAttach(dpy, &c->shm);

  int errorBase;
  XDamageQueryExtension(dpy, &c->damageEventBase, &errorBase);
  c->damage = XDamageCreate(dpy, root, XDamageReportBoundingBox);

  return c;
}

void capturer_destroy(Capturer *c) {
  if (!c) return;
  XDamageDestroy(c->dpy, c->damage);
  XShmDetach(c->dpy, &c->shm);
  XDestroyImage(c->img);
  shmdt(c->shm.shmaddr);
  shmctl(c->shm.shmid, IPC_RMID, NULL);
  free(c);
}

void capturer_run(Capturer *c, OnFrameCallback onFrame, void *userdata) {
  XEvent ev;

  while (1) {
    XNextEvent(c->dpy, &ev);

    if (ev.type != c->damageEventBase + XDamageNotify)
      continue;

    XDamageNotifyEvent *dev = (XDamageNotifyEvent *)&ev;

    int x = dev->area.x;
    int y = dev->area.y;
    int w = dev->area.width;
    int h = dev->area.height;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > c->width)  w = c->width  - x;
    if (y + h > c->height) h = c->height - y;
    if (w <= 0 || h <= 0)
      goto reset;

    if (!XShmGetImage(c->dpy, c->root, c->img, 0, 0, AllPlanes)) {
      printf("XShmGetImage failed\n");
      goto reset;
    }

    DirtyFrame frame = {
        .x = x, .y = y, .w = w, .h = h,
        .pixels = (uint8_t *)c->img->data + y * c->img->bytes_per_line + x * 4,
        .stride = c->img->bytes_per_line,
    };

    onFrame(&frame, userdata);

  reset:
    XDamageSubtract(c->dpy, c->damage, None, None);
  }
}
