#include "stream.h"
#include "debug.h"
#include "network.h"
#include <lz4.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UDP_MAX_PAYLOAD 60000
#define HEADER_BYTES    ((int)(sizeof(int) * 5))

static void send_strip(UdpSession *session, DirtyFrame *frame) {
  const int pixelBytes = frame->h * frame->w * 4;
  uint8_t *pixels = malloc(pixelBytes);
  uint8_t *data   = malloc(HEADER_BYTES + LZ4_COMPRESSBOUND(pixelBytes));
  if (!pixels || !data) { free(pixels); free(data); return; }

  for (int r = 0; r < frame->h; r++)
    memcpy(pixels + r * frame->w * 4,
           frame->pixels + r * frame->stride,
           frame->w * 4);

  int compSize = LZ4_compress_fast((char *)pixels, (char *)(data + HEADER_BYTES),
                                   pixelBytes, LZ4_COMPRESSBOUND(pixelBytes), 1);
  free(pixels);
  if (compSize <= 0) { free(data); return; }

  int header[5] = {frame->w, frame->h, frame->x, frame->y, pixelBytes};
  memcpy(data, header, HEADER_BYTES);

  udp_send(session, (char *)data, HEADER_BYTES + compSize);
  free(data);
}

void stream_on_frame(DirtyFrame *frame, void *userdata) {
  UdpSession *session = (UdpSession *)userdata;
  if (!session->connected) return;

  debug_show(frame);

  const int pixelBytes = frame->h * frame->w * 4;

  if (HEADER_BYTES + LZ4_COMPRESSBOUND(pixelBytes) <= UDP_MAX_PAYLOAD) {
    send_strip(session, frame);
    return;
  }

  int max_rows = (UDP_MAX_PAYLOAD - HEADER_BYTES) / (frame->w * 4);
  if (max_rows < 1) max_rows = 1;
  int n_chunks = (frame->h + max_rows - 1) / max_rows;

#ifdef DEBUG
  fprintf(stderr, "[stream] frame %dx%d@(%d,%d) too large, splitting into %d strips of %d rows\n",
          frame->w, frame->h, frame->x, frame->y, n_chunks, max_rows);
#endif

  for (int i = 0; i < n_chunks; i++) {
    int y0 = i * max_rows;
    int h  = (y0 + max_rows > frame->h) ? frame->h - y0 : max_rows;
    DirtyFrame strip = {
      .x      = frame->x,
      .y      = frame->y + y0,
      .w      = frame->w,
      .h      = h,
      .pixels = frame->pixels + y0 * frame->stride,
      .stride = frame->stride,
    };
    send_strip(session, &strip);
    if (i < n_chunks - 1) usleep(1000);
  }
}
