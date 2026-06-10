#pragma once
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stddef.h>

void input_init(Display *dpy);
void on_command(int command, size_t data_size, char *data);

void pressEnter(Display *dpy);
void typeChar(Display *dpy, char c);
void typeString(Display *dpy, const char *str, int shouldPressEnter);
