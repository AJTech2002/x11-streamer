#include "display.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *active_dpy = NULL;
static Window active_root = 0;

Display *display_open(void) {
  active_dpy = XOpenDisplay(NULL);
  if (!active_dpy) {
    fprintf(stderr, "Failed to open display (DISPLAY=%s)\n",
            getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
    return NULL;
  }
  active_root = DefaultRootWindow(active_dpy);
  return active_dpy;
}

void display_close(void) {
  if (active_dpy) {
    XCloseDisplay(active_dpy);
    active_dpy = NULL;
    active_root = 0;
  }
}

void display_set_resolution(int width, int height) {
  if (!active_dpy) return;

  XRRScreenResources *res = XRRGetScreenResources(active_dpy, active_root);
  if (!res) {
    fprintf(stderr, "XRandR: failed to get screen resources\n");
    return;
  }

  for (int i = 0; i < res->noutput; i++) {
    XRROutputInfo *out = XRRGetOutputInfo(active_dpy, res, res->outputs[i]);
    if (!out) continue;

    if (out->connection != RR_Connected || !out->crtc) {
      XRRFreeOutputInfo(out);
      continue;
    }

    XRRCrtcInfo *crtc = XRRGetCrtcInfo(active_dpy, res, out->crtc);
    if (!crtc) {
      XRRFreeOutputInfo(out);
      continue;
    }

    // Search mode in the output's own supported mode list, not the global list.
    // Cross-reference with res->modes to get dimensions.
    RRMode mode_id = 0;
    for (int j = 0; j < out->nmode; j++) {
      for (int m = 0; m < res->nmode; m++) {
        if (res->modes[m].id == out->modes[j] &&
            (int)res->modes[m].width == width &&
            (int)res->modes[m].height == height) {
          mode_id = res->modes[m].id;
          break;
        }
      }
      if (mode_id) break;
    }

    if (mode_id) {
      // Must use res->configTimestamp, not CurrentTime — XRandR rejects stale
      // or zero timestamps to guard against concurrent config changes.
      Status s = XRRSetCrtcConfig(active_dpy, res, out->crtc,
                                  res->configTimestamp,
                                  crtc->x, crtc->y, mode_id, crtc->rotation,
                                  crtc->outputs, crtc->noutput);
      if (s == RRSetConfigSuccess)
        printf("Resolution set to %dx%d on output %s\n", width, height,
               out->name);
      else
        fprintf(stderr, "XRandR: failed to set %dx%d on %s\n", width, height,
                out->name);
    } else {
      fprintf(stderr, "XRandR: no mode %dx%d found for %s\n", width, height,
              out->name);
    }

    XRRFreeCrtcInfo(crtc);
    XRRFreeOutputInfo(out);
    break; // apply to first connected output only
  }

  XRRFreeScreenResources(res);
  XFlush(active_dpy);
}

int screenshot(const char *path) {
  XWindowAttributes attrs;
  XGetWindowAttributes(active_dpy, active_root, &attrs);
  int w = attrs.width;
  int h = attrs.height;

  XImage *img =
      XGetImage(active_dpy, active_root, 0, 0, w, h, AllPlanes, ZPixmap);
  if (!img) {
    fprintf(stderr, "XGetImage failed\n");
    return 1;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    XDestroyImage(img);
    return 1;
  }

  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned long pixel = XGetPixel(img, x, y);
      fputc((pixel >> 16) & 0xFF, f);
      fputc((pixel >> 8) & 0xFF, f);
      fputc((pixel >> 0) & 0xFF, f);
    }
  }

  fclose(f);
  XDestroyImage(img);
  printf("Screenshot saved to %s (%dx%d)\n", path, w, h);
  return 0;
}

void focusWindow(Window w) {
  XRaiseWindow(active_dpy, w);
  XSetInputFocus(active_dpy, w, RevertToParent, CurrentTime);
  XFlush(active_dpy);
  usleep(100000);
}

void printAllWindows(Window root) {
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(active_dpy, root, &root, &parent, &children, &nChildren);
  for (unsigned int i = 0; i < nChildren; i++) {
    char *name = NULL;
    XFetchName(active_dpy, children[i], &name);

    if (!name) {
      Atom actualType;
      int actualFormat;
      unsigned long nItems, bytesAfter;
      unsigned char *prop = NULL;
      XGetWindowProperty(active_dpy, children[i],
                         XInternAtom(active_dpy, "WM_NAME", False), 0, 1024,
                         False, AnyPropertyType, &actualType, &actualFormat,
                         &nItems, &bytesAfter, &prop);
      if (prop) {
        name = strdup((char *)prop);
        XFree(prop);
      }
    }

    printf("window: %lu — %s\n", children[i], name ? name : "(no name)");
    if (name) free(name);
    printAllWindows(children[i]);
  }
  if (children) XFree(children);
}

void printAllWindowsByClass(Window root) {
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(active_dpy, root, &root, &parent, &children, &nChildren);
  for (unsigned int i = 0; i < nChildren; i++) {
    XClassHint hint;
    if (XGetClassHint(active_dpy, children[i], &hint)) {
      printf("window: %lu — name: %s  class: %s\n", children[i], hint.res_name,
             hint.res_class);
      XFree(hint.res_name);
      XFree(hint.res_class);
    }
    printAllWindowsByClass(children[i]);
  }
  if (children) XFree(children);
}

Window findWindowByName(Window root, const char *name) {
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(active_dpy, root, &root, &parent, &children, &nChildren);

  Window result = 0;
  for (unsigned int i = 0; i < nChildren; i++) {
    char *winName = NULL;
    XFetchName(active_dpy, children[i], &winName);
    if (winName) {
      if (strstr(winName, name)) {
        result = children[i];
        XFree(winName);
        break;
      }
      XFree(winName);
    }
    result = findWindowByName(children[i], name);
    if (result) break;
  }
  if (children) XFree(children);
  return result;
}

Window findWindowByClass(Window root, const char *className) {
  Window parent, *children;
  unsigned int nChildren;

  XQueryTree(active_dpy, root, &root, &parent, &children, &nChildren);

  Window result = 0;
  for (unsigned int i = 0; i < nChildren; i++) {
    XClassHint hint;
    if (XGetClassHint(active_dpy, children[i], &hint)) {
      int match = strstr(hint.res_class, className) != NULL;
      XFree(hint.res_name);
      XFree(hint.res_class);
      if (match) {
        result = children[i];
        break;
      }
    }
    result = findWindowByClass(children[i], className);
    if (result) break;
  }
  if (children) XFree(children);
  return result;
}
