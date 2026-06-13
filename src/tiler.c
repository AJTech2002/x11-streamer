#include "capture.h"
#include "tile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

inline unsigned int tile_idx(const unsigned int x, const unsigned int y) {
  return y * TILE_SIZE + x;
}

void tiler_init(FragScreen *screen, int resX, int resY) {
  screen->resX = resX;
  screen->resY = resY;

  screen->tilesX = ceil((float)resX / TILE_SIZE);
  screen->tilesY = ceil((float)resY / TILE_SIZE);

  screen->mask_size = sizeof(uint8_t) * resX * resY;
  screen->pixel_size = sizeof(uint8_t) * resX * resY;

  screen->pixels = malloc(screen->pixel_size);
  screen->mask = malloc(screen->mask_size);

  printf("Created tiler with %d x %d tiles", screen->tilesX, screen->tilesY);
};

void tiler_dispose(FragScreen *screen) {
  free(screen->pixels);
  free(screen->mask);
  free(screen);
};

void tiler_encode(FragScreen *screen, const DirtyFrame *frame) {
  int startingTileX = (int)floor((float)frame->x / TILE_SIZE);
  int startingTileY = (int)floor((float)frame->y / TILE_SIZE);
  int endTileX = (int)floor(((float)frame->x + (float)frame->w) / TILE_SIZE);
  int endTileY = (int)floor(((float)frame->y + (float)frame->h) / TILE_SIZE);

  printf("Dirty from %d, %d -> %d, %d", startingTileX, startingTileY, endTileX,
         endTileY);

  // TODO: Optimize with memset
  for (int x = startingTileX; x < endTileX; x++) {
    for (int y = startingTileY; y < endTileY; y++) {
      screen->mask[tile_idx(x, y)] = 1;
    }
  }
}