#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "options.h"

typedef struct {
    int xvfbPid;
    Display* dpy;
    Window root;
} XVFB;

XVFB* init();
int end();
int screenshot (const char* path);