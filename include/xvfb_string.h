#pragma once
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <string.h>
#include <unistd.h>

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
    {'~', "asciitilde", 1},  {0, NULL, 0} // sentinel
};

void pressEnter(Display *dpy) {
  KeyCode code = XKeysymToKeycode(dpy, XK_Return);
  XTestFakeKeyEvent(dpy, code, True, 0);
  XTestFakeKeyEvent(dpy, code, False, 0);
  XFlush(dpy);
}

void typeChar(Display *dpy, char c) {
  KeySym sym = NoSymbol;
  int needsShift = 0;

  // look up in charMap first
  for (int i = 0; charMap[i].c != 0; i++) {
    if (charMap[i].c == c) {
      sym = XStringToKeysym(charMap[i].keysymName);
      needsShift = charMap[i].needsShift;
      break;
    }
  }

  // uppercase letters
  if (sym == NoSymbol && c >= 'A' && c <= 'Z') {
    char lower[2] = {c + 32, 0};
    sym = XStringToKeysym(lower);
    needsShift = 1;
  }

  // lowercase letters and digits — direct lookup
  if (sym == NoSymbol) {
    char tmp[2] = {c, 0};
    sym = XStringToKeysym(tmp);
  }

  if (sym == NoSymbol) {
    return;
  }

  KeyCode code = XKeysymToKeycode(dpy, sym);
  KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);

  if (!code) {
    return;
  }

  if (needsShift)
    XTestFakeKeyEvent(dpy, shift, True, 0);
  XTestFakeKeyEvent(dpy, code, True, 0);
  XTestFakeKeyEvent(dpy, code, False, 0);
  if (needsShift)
    XTestFakeKeyEvent(dpy, shift, False, 0);
  XFlush(dpy);
}

void typeString(Display *dpy, const char *str, int shouldPressEnter) {
  for (int i = 0; i < (int)strlen(str); i++) {
    typeChar(dpy, str[i]);
    usleep(100000); // 10ms between chars — some apps miss fast input
  }
  if (shouldPressEnter)
    pressEnter(dpy);
}
