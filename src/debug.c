#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DebugWindow *dbg = NULL;

void debug_create(int w, int h) {
  dbg = calloc(1, sizeof(DebugWindow));
  dbg->w = w;
  dbg->h = h;

  dbg->dpy = XOpenDisplay(NULL);
  if (!dbg->dpy) {
    fprintf(stderr, "debug: cant open display\n");
    return;
  }

  int screen = DefaultScreen(dbg->dpy);
  dbg->win = XCreateSimpleWindow(dbg->dpy, DefaultRootWindow(dbg->dpy), 0, 0, w, h,
                                 0, BlackPixel(dbg->dpy, screen), BlackPixel(dbg->dpy, screen));
  XStoreName(dbg->dpy, dbg->win, "xvfb debug");
  XMapWindow(dbg->dpy, dbg->win);
  dbg->gc = XCreateGC(dbg->dpy, dbg->win, 0, NULL);

  dbg->img = XShmCreateImage(dbg->dpy, DefaultVisual(dbg->dpy, screen),
                             DefaultDepth(dbg->dpy, screen), ZPixmap, NULL, &dbg->shm, w, h);

  dbg->shm.shmid = shmget(IPC_PRIVATE, dbg->img->bytes_per_line * h, IPC_CREAT | 0777);
  dbg->shm.shmaddr = dbg->img->data = shmat(dbg->shm.shmid, NULL, 0);
  dbg->shm.readOnly = False;
  XShmAttach(dbg->dpy, &dbg->shm);
  XFlush(dbg->dpy);
}

void debug_show(DirtyFrame *frame) {
  if (!dbg) return;

  for (int row = 0; row < frame->h; row++) {
    memcpy(dbg->img->data + (frame->y + row) * dbg->img->bytes_per_line + frame->x * 4,
           frame->pixels + row * frame->stride, frame->w * 4);
  }

  XShmPutImage(dbg->dpy, dbg->win, dbg->gc, dbg->img, 0, 0, 0, 0, dbg->w, dbg->h, False);
  XFlush(dbg->dpy);
}

void debug_destroy(void) {
  if (!dbg) return;
  XShmDetach(dbg->dpy, &dbg->shm);
  XDestroyImage(dbg->img);
  shmdt(dbg->shm.shmaddr);
  shmctl(dbg->shm.shmid, IPC_RMID, NULL);
  XDestroyWindow(dbg->dpy, dbg->win);
  XFreeGC(dbg->dpy, dbg->gc);
  XCloseDisplay(dbg->dpy);
  free(dbg);
  dbg = NULL;
}
