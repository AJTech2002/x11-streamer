#include "xvfb_streamer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Capturer* capturer_create(Display *dpy, Window root, int w, int h) 
{
    Capturer* c = calloc(1, sizeof(Capturer));
    c->dpy    = dpy;
    c->root   = root;
    c->width  = w;
    c->height = h;

    // ── SHM image — reused every capture, no malloc in loop
    c->img = XShmCreateImage(dpy,
        DefaultVisual(dpy, 0),
        DefaultDepth(dpy, 0),
        ZPixmap, NULL, &c->shm, w, h);

    c->shm.shmid   = shmget(IPC_PRIVATE,
                                c->img->bytes_per_line * h,
                                IPC_CREAT | 0777);
    c->shm.shmaddr = c->img->data = shmat(c->shm.shmid, NULL, 0);
    c->shm.readOnly = False;
    XShmAttach(dpy, &c->shm);

    // ── XDamage subscription
    int errorBase;
    XDamageQueryExtension(dpy, &c->damageEventBase, &errorBase);
    c->damage = XDamageCreate(dpy, root, XDamageReportBoundingBox);

    return c;
}

void capturer_destroy(Capturer* c) {
    if (!c) return;
    XDamageDestroy(c->dpy, c->damage);
    XShmDetach(c->dpy, &c->shm);
    XDestroyImage(c->img);
    shmdt(c->shm.shmaddr);
    shmctl(c->shm.shmid, IPC_RMID, NULL);
    free(c);
}

void capturer_run(Capturer* c, OnFrameCallback onFrame, void* userdata) {
    XEvent ev;

    while (1) {
        XNextEvent(c->dpy, &ev);

        if (ev.type != c->damageEventBase + XDamageNotify) continue;

        XDamageNotifyEvent* dev = (XDamageNotifyEvent*)&ev;

        int x = dev->area.x;
        int y = dev->area.y;
        int w = dev->area.width;
        int h = dev->area.height;

        // clamp
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + w > c->width)  w = c->width  - x;
        if (y + h > c->height) h = c->height - y;
        if (w <= 0 || h <= 0) goto reset;

        // capture full screen from 0,0 — SHM image must match full dimensions
        // dirty rect x,y,w,h tells you what changed within it
        if (!XShmGetImage(c->dpy, c->root, c->img, 0, 0, AllPlanes)) {
            printf("XShmGetImage failed\n");
            goto reset;
        }
        

        DirtyFrame frame = {
            .x      = x,
            .y      = y,
            .w      = w,
            .h      = h,
            // offset pixels pointer to start of dirty rect
            .pixels = (uint8_t*)c->img->data
                      + y * c->img->bytes_per_line
                      + x * 4,
            .stride = c->img->bytes_per_line,
        };

        onFrame(&frame, userdata);

    reset:
        XDamageSubtract(c->dpy, c->damage, None, None);
    }
}

// ===== DEBUGGING ======
DebugWindow* dbg = NULL;

void debug_create(int w, int h) {
    dbg = calloc(1, sizeof(DebugWindow));
    dbg->w = w;
    dbg->h = h;

    dbg->dpy = XOpenDisplay(NULL);  // NULL = $DISPLAY = your Hyprland
    if (!dbg->dpy) { fprintf(stderr, "debug: cant open display\n"); return; }

    int screen = DefaultScreen(dbg->dpy);

    dbg->win = XCreateSimpleWindow(dbg->dpy,
        DefaultRootWindow(dbg->dpy),
        0, 0, w, h, 0,
        BlackPixel(dbg->dpy, screen),
        BlackPixel(dbg->dpy, screen));

    XStoreName(dbg->dpy, dbg->win, "xvfb debug");
    XMapWindow(dbg->dpy, dbg->win);
    dbg->gc = XCreateGC(dbg->dpy, dbg->win, 0, NULL);

    dbg->img = XShmCreateImage(dbg->dpy,
        DefaultVisual(dbg->dpy, screen),
        DefaultDepth(dbg->dpy, screen),
        ZPixmap, NULL, &dbg->shm, w, h);

    dbg->shm.shmid   = shmget(IPC_PRIVATE,
                               dbg->img->bytes_per_line * h,
                               IPC_CREAT | 0777);
    dbg->shm.shmaddr = dbg->img->data = shmat(dbg->shm.shmid, NULL, 0);
    dbg->shm.readOnly = False;
    XShmAttach(dbg->dpy, &dbg->shm);
    XFlush(dbg->dpy);
}


void debug_show(DirtyFrame* frame) {
    if (!dbg) return;

    // copy dirty rect into debug image at correct position
    for (int row = 0; row < frame->h; row++) {
        memcpy(
            dbg->img->data + (frame->y + row) * dbg->img->bytes_per_line + frame->x * 4,
            frame->pixels  + row * frame->stride,
            frame->w * 4
        );
    }

    XShmPutImage(dbg->dpy, dbg->win, dbg->gc, dbg->img,
        0, 0, 0, 0, dbg->w, dbg->h, False);
    XFlush(dbg->dpy);
}

void debug_destroy() {
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
