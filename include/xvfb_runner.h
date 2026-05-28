#pragma once
#include "options.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
  int xvfbPid;
  Display *dpy;
  Window root;
} XVFB;

XVFB *xvfb_init();
int xvfb_end();
int screenshot(const char *path);

int xvfb_clear_display();
void focusWindow(Window w);
Window findWindowByName(Window root, const char *name);
void printAllWindows(Window root);
void printAllWindowsByClass(Window root);
Window findWindowByClass(Window root, const char *className);
