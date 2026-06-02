#include "xvfb_runner.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int xvfbPid = 0;
static XVFB *active = NULL;

void xvfb_stop() {
  if (xvfbPid > 0) {
    kill(xvfbPid, SIGTERM);
    waitpid(xvfbPid, NULL, 0);
    xvfbPid = 0; // ← back to 0 not -1
  }
  // always clean lock file
  system("rm -f /tmp/.X90-lock");
  usleep(100000);
};

void xvfb_start() {
  xvfb_stop();

  xvfbPid = fork();
  if (xvfbPid == 0) {
    freopen("/dev/null", "w", stderr);
    execlp("Xvfb", "Xvfb", DISPLAY_STR, "-screen", "0", "1280x720x24", "-ac",
           "+extension", "GLX", NULL);
    _exit(1);
  }

  // wait for display to be ready
  Display *dpy = NULL;
  for (int i = 0; i < 30; i++) {
    usleep(200000);
    dpy = XOpenDisplay(DISPLAY_STR);
    if (dpy) {
      XCloseDisplay(dpy);
      printf("Xvfb ready on %s\n", DISPLAY_STR);
      return;
    }
  }
  fprintf(stderr, "Xvfb failed to start\n");
  exit(1);
};

XVFB *xvfb_init() {
  active = calloc(1, sizeof(XVFB));
  if (!active)
    return NULL;

  xvfb_start();

  for (int i = 0; i < 10; i++) {
    active->dpy = XOpenDisplay(DISPLAY_STR);
    if (active->dpy)
      break;
    usleep(200000);
  }
  if (!active->dpy) {
    fprintf(stderr, "Failed to open display\n");
    free(active);
    active = NULL;
    xvfb_stop();
    return NULL;
  }

  // set root BEFORE calling anything that uses it
  active->root = DefaultRootWindow(active->dpy);

  xvfb_clear_display(); // ← now safe, root is valid
  usleep(200000);

  return active;
}

int xvfb_end() {
  if (active && active->dpy) {
    XCloseDisplay(active->dpy);
    xvfb_stop();
  }

  return 0;
};

int xvfb_clear_display() {

  if (!active || !active->dpy) {
    return 1;
  }

  Window parent, root, *children;
  unsigned int nChildren;

  Display *dpy = active->dpy;

  XQueryTree(dpy, active->root, &active->root, &parent, &children, &nChildren);

  for (unsigned int i = 0; i < nChildren; i++) {
    XDestroyWindow(dpy, children[i]);
  }

  if (children)
    XFree(children);
  XFlush(dpy);
  return 0;
}

int screenshot(const char *path) {
  XWindowAttributes attrs;

  Display *dpy = active->dpy;
  Window root = active->root;

  XGetWindowAttributes(dpy, root, &attrs);
  int w = attrs.width;
  int h = attrs.height;

  // capture entire root window
  XImage *img = XGetImage(dpy, root, 0, 0, w, h, AllPlanes, ZPixmap);
  if (!img) {
    fprintf(stderr, "XGetImage failed\n");
    return 1;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    XDestroyImage(img);
    return 1;
  }

  // PPM header
  fprintf(f, "P6\n%d %d\n255\n", w, h);

  // write pixels — XImage is BGRA, PPM wants RGB
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned long pixel = XGetPixel(img, x, y);
      unsigned char r = (pixel >> 16) & 0xFF;
      unsigned char g = (pixel >> 8) & 0xFF;
      unsigned char b = (pixel >> 0) & 0xFF;
      fputc(r, f);
      fputc(g, f);
      fputc(b, f);
    }
  }

  fclose(f);
  XDestroyImage(img);
  printf("Screenshot saved to %s (%dx%d)\n", path, w, h);

  return 0;
}

void focusWindow(Window w) {
  XRaiseWindow(active->dpy, w);
  XSetInputFocus(active->dpy, w, RevertToParent, CurrentTime);
  XFlush(active->dpy);
  usleep(100000);
}

void printAllWindows(Window root) {
  Display *dpy = active->dpy;
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(dpy, root, &root, &parent, &children, &nChildren);

  for (unsigned int i = 0; i < nChildren; i++) {
    // try XFetchName first
    char *name = NULL;
    XFetchName(dpy, children[i], &name);

    // fallback to WM_NAME property
    if (!name) {
      Atom actualType;
      int actualFormat;
      unsigned long nItems, bytesAfter;
      unsigned char *prop = NULL;

      XGetWindowProperty(dpy, children[i], XInternAtom(dpy, "WM_NAME", False),
                         0, 1024, False, AnyPropertyType, &actualType,
                         &actualFormat, &nItems, &bytesAfter, &prop);

      if (prop) {
        name = strdup((char *)prop);
        XFree(prop);
      }
    }

    // print even if no name — shows window ID
    printf("window: %lu — %s\n", children[i], name ? name : "(no name)");
    if (name)
      free(name);

    printAllWindows(children[i]);
  }

  if (children)
    XFree(children);
};

Window findWindowByName(Window root, const char *name) {
  Window parent, *children;
  Display *dpy = active->dpy;

  unsigned int nChildren;

  XQueryTree(dpy, root, &root, &parent, &children, &nChildren);

  Window result = 0;
  for (unsigned int i = 0; i < nChildren; i++) {
    char *winName = NULL;
    XFetchName(dpy, children[i], &winName);

    if (winName) {
      if (strstr(winName, name)) { // partial match
        result = children[i];
        XFree(winName);
        break;
      }
      XFree(winName);
    }

    // recurse into children
    result = findWindowByName(children[i], name);
    if (result)
      break;
  }

  if (children)
    XFree(children);
  return result;
}

void printAllWindowsByClass(Window root) {
  Display *dpy = active->dpy;
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(dpy, root, &root, &parent, &children, &nChildren);

  for (unsigned int i = 0; i < nChildren; i++) {
    XClassHint hint;
    if (XGetClassHint(dpy, children[i], &hint)) {
      printf("window: %lu — name: %s  class: %s\n", children[i], hint.res_name,
             hint.res_class);
      XFree(hint.res_name);
      XFree(hint.res_class);
    }

    printAllWindowsByClass(children[i]);
  }

  if (children)
    XFree(children);
}

Window findWindowByClass(Window root, const char *className) {
  Display *dpy = active->dpy;
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(dpy, root, &root, &parent, &children, &nChildren);

  Window result = 0;
  for (unsigned int i = 0; i < nChildren; i++) {
    XClassHint hint;
    if (XGetClassHint(dpy, children[i], &hint)) {
      int match = strstr(hint.res_class, className) != NULL;
      XFree(hint.res_name);
      XFree(hint.res_class);
      if (match) {
        result = children[i];
        break;
      }
    }

    result = findWindowByClass(children[i], className);
    if (result)
      break;
  }

  if (children)
    XFree(children);
  return result;
}
