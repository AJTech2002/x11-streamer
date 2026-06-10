#pragma once
#include "options.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
  int xvfbPid;
  Display *dpy;
  Window root;
} XVFB;

XVFB *xvfb_init(void);
int xvfb_end(void);
int xvfb_clear_display(void);
int screenshot(const char *path);

void focusWindow(Window w);
Window findWindowByName(Window root, const char *name);
Window findWindowByClass(Window root, const char *className);
void printAllWindows(Window root);
void printAllWindowsByClass(Window root);
