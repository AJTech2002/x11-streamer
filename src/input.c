#include "input.h"
#include "logs.h"
#include <X11/XKBlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SCREEN_WIDTH  1280.0
#define SCREEN_HEIGHT 720.0

#define COMMAND_MOUSE_CLICK 2
#define COMMAND_MOUSE_MOVE  3

typedef struct {
  char c;
  const char *keysymName;
  int needsShift;
} CharMap;

static const CharMap charMap[] = {
    {' ', "space", 0},       {'!', "exclam", 1},
    {'"', "quotedbl", 1},    {'#', "numbersign", 1},
    {'$', "dollar", 1},      {'%', "percent", 1},
    {'&', "ampersand", 1},   {'\'', "apostrophe", 0},
    {'(', "parenleft", 1},   {')', "parenright", 1},
    {'*', "asterisk", 1},    {'+', "plus", 1},
    {',', "comma", 0},       {'-', "minus", 0},
    {'.', "period", 0},      {'/', "slash", 0},
    {':', "colon", 1},       {';', "semicolon", 0},
    {'<', "less", 1},        {'=', "equal", 0},
    {'>', "greater", 1},     {'?', "question", 1},
    {'@', "at", 1},          {'[', "bracketleft", 0},
    {'\\', "backslash", 0},  {']', "bracketright", 0},
    {'^', "asciicircum", 1}, {'_', "underscore", 1},
    {'`', "grave", 0},       {'{', "braceleft", 1},
    {'|', "bar", 1},         {'}', "braceright", 1},
    {'~', "asciitilde", 1},  {0, NULL, 0},
};

static Display *cmd_dpy = NULL;
static bool currently_pressed = false;

void input_init(Display *dpy) {
  cmd_dpy = dpy;
}

void on_command(int command, size_t data_size, char *data) {
  (void)data_size;

  typedef struct {
    float percX;
    float percY;
    bool down;
  } ClickPayload;

  Window root = RootWindow(cmd_dpy, DefaultScreen(cmd_dpy));

  if (command == COMMAND_MOUSE_CLICK) {
    ClickPayload *p = (ClickPayload *)data;
    int x = (int)round(SCREEN_WIDTH  * p->percX);
    int y = (int)round(SCREEN_HEIGHT * p->percY);

    XWarpPointer(cmd_dpy, None, root, 0, 0, 0, 0, x, y);
    XFlush(cmd_dpy);
    LOG_INFO_SBJ("click", "[%f, %f]", p->percX, p->percY);
    XTestFakeButtonEvent(cmd_dpy, 1, p->down, CurrentTime);
    XFlush(cmd_dpy);
    currently_pressed = p->down;

  } else if (command == COMMAND_MOUSE_MOVE) {
    ClickPayload *p = (ClickPayload *)data;
    int x = (int)round(SCREEN_WIDTH  * p->percX);
    int y = (int)round(SCREEN_HEIGHT * p->percY);

    if (p->down != currently_pressed) {
      XTestFakeButtonEvent(cmd_dpy, 1, p->down, CurrentTime);
      XFlush(cmd_dpy);
      currently_pressed = p->down;
    }

    XWarpPointer(cmd_dpy, None, root, 0, 0, 0, 0, x, y);
    XFlush(cmd_dpy);
  }
}

void pressEnter(Display *dpy) {
  KeyCode code = XKeysymToKeycode(dpy, XK_Return);
  XTestFakeKeyEvent(dpy, code, True, 0);
  XTestFakeKeyEvent(dpy, code, False, 0);
  XFlush(dpy);
}

void typeChar(Display *dpy, char c) {
  KeySym sym = NoSymbol;
  int needsShift = 0;

  for (int i = 0; charMap[i].c != 0; i++) {
    if (charMap[i].c == c) {
      sym = XStringToKeysym(charMap[i].keysymName);
      needsShift = charMap[i].needsShift;
      break;
    }
  }

  if (sym == NoSymbol && c >= 'A' && c <= 'Z') {
    char lower[2] = {c + 32, 0};
    sym = XStringToKeysym(lower);
    needsShift = 1;
  }

  if (sym == NoSymbol) {
    char tmp[2] = {c, 0};
    sym = XStringToKeysym(tmp);
  }

  if (sym == NoSymbol) return;

  KeyCode code = XKeysymToKeycode(dpy, sym);
  KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);
  if (!code) return;

  if (needsShift) XTestFakeKeyEvent(dpy, shift, True, 0);
  XTestFakeKeyEvent(dpy, code, True, 0);
  XTestFakeKeyEvent(dpy, code, False, 0);
  if (needsShift) XTestFakeKeyEvent(dpy, shift, False, 0);
  XFlush(dpy);
}

void typeString(Display *dpy, const char *str, int shouldPressEnter) {
  for (int i = 0; i < (int)strlen(str); i++) {
    typeChar(dpy, str[i]);
    usleep(100000);
  }
  if (shouldPressEnter) pressEnter(dpy);
}
