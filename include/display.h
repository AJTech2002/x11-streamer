#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>

Display *display_open(void);
void display_close(void);
void display_set_resolution(int width, int height);
int screenshot(const char *path);

void focusWindow(Window w);
Window findWindowByName(Window root, const char *name);
Window findWindowByClass(Window root, const char *className);
void printAllWindows(Window root);
void printAllWindowsByClass(Window root);
