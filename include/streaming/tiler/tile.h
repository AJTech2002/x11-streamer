#pragma once
#include <stdbool.h>

struct Tile {
  int x, y;
  bool dirty;
};

struct Screen {
  int resX, resY;
  int tilesX, tilesY;
  struct Tile *tiles;
};
