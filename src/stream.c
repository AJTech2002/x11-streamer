#include "stream.h"
#include "debug.h"
#include "network.h"
#include <lz4.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void stream_on_frame(DirtyFrame *frame, void *userdata) {
  UdpSession *session = (UdpSession *)userdata;
  if (!session->connected) return;

  debug_show(frame);

  const int pixelBytes = frame->h * frame->w * 4;
  const unsigned int headerSize = sizeof(int) * 5;

  uint8_t *pixels = malloc(pixelBytes);
  uint8_t *data = malloc(headerSize + LZ4_COMPRESSBOUND(pixelBytes));
  if (!pixels || !data) {
    free(pixels);
    free(data);
    return;
  }

  for (int r = 0; r < frame->h; r++) {
    memcpy(pixels + r * frame->w * 4,
           frame->pixels + r * frame->stride,
           frame->w * 4);
  }

  int compSize = LZ4_compress_fast((char *)pixels, (char *)(data + headerSize),
                                   pixelBytes, LZ4_COMPRESSBOUND(pixelBytes), 1);
  free(pixels);
  if (compSize <= 0) {
    free(data);
    return;
  }

  int header[5] = {frame->w, frame->h, frame->x, frame->y, pixelBytes};
  memcpy(data, header, headerSize);

  udp_send(session, (char *)data, headerSize + compSize);
  free(data);
}
