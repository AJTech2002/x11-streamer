#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TILE_SIZE 64.0f // 64x64 tile size to fit into UDP packet

typedef struct DirtyFrame DirtyFrame;

typedef struct FragScreen {
  int resX, resY;
  int tilesX, tilesY;
  uint8_t *pixels;
  uint8_t *mask;
  size_t mask_size;
  size_t pixel_size;
} FragScreen;

void tiler_init(FragScreen *screen, int resX, int resY);
void tiler_encode(FragScreen *screen, const DirtyFrame *frame);
