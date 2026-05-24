#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdamage.h>
#include <stdint.h>
#include <sys/shm.h>

typedef struct {
    Display*        dpy;
    Window          root;
    XShmSegmentInfo shm;
    XImage*         img;
    Damage          damage;
    int             damageEventBase;
    int             width;
    int             height;
} Capturer;

// ── INIT ─────────────────────────────────────────────────

Capturer* capturer_create(Display* dpy, Window root, int w, int h);
void capturer_destroy(Capturer* c);

// ── CAPTURE LOOP ─────────────────────────────────────────

typedef struct {
    int     x, y, w, h;   // dirty region
    uint8_t* pixels;       // raw BGRA — points into SHM, do not free
    int     stride;        // bytes per row
} DirtyFrame;

// callback — called whenever a dirty frame is captured

typedef void (*OnFrameCallback)(DirtyFrame* frame, void* userdata);
void capturer_run(Capturer* c, OnFrameCallback onFrame, void* userdata);

// --- DEBUGGING ---

typedef struct {
    Display*        dpy;
    Window          win;
    GC              gc;
    XImage*         img;
    XShmSegmentInfo shm;
    int             w, h;
} DebugWindow;


void debug_create(int w, int h);

void debug_show(DirtyFrame* frame);

void debug_destroy();