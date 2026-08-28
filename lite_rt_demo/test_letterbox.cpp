// Round-trip test: project a known source box forward into letterboxed
// normalized space by hand, then check undo_letterbox() recovers it.
#include "letterbox.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static bool close(float a, float b, float eps = 0.6f) {
  return std::fabs(a - b) < eps;
}

static void roundtrip(int sw, int sh, int dw, int dh,
                      float x0, float y0, float x1, float y1) {
  LetterboxParams p = compute_letterbox(sw, sh, dw, dh);

  // Forward: source px -> canvas px -> normalized.
  float nxmin = (x0 * p.scale + p.pad_x) / p.dst_w;
  float nymin = (y0 * p.scale + p.pad_y) / p.dst_h;
  float nxmax = (x1 * p.scale + p.pad_x) / p.dst_w;
  float nymax = (y1 * p.scale + p.pad_y) / p.dst_h;

  BoxPx b = undo_letterbox(p, nymin, nxmin, nymax, nxmax);
  printf("  %dx%d -> %dx%d  scale=%.4f pad=(%d,%d) new=(%d,%d)\n",
         sw, sh, dw, dh, p.scale, p.pad_x, p.pad_y, p.new_w, p.new_h);
  printf("    in (%.1f,%.1f,%.1f,%.1f) out (%.1f,%.1f,%.1f,%.1f)\n",
         x0, y0, x1, y1, b.x0, b.y0, b.x1, b.y1);
  assert(close(b.x0, x0) && close(b.y0, y0));
  assert(close(b.x1, x1) && close(b.y1, y1));
}

int main() {
  printf("landscape 16:9 into square\n");
  roundtrip(1280, 720, 320, 320, 100, 50, 800, 600);

  printf("portrait into square\n");
  roundtrip(720, 1280, 320, 320, 60, 200, 640, 1100);

  printf("already square (no padding expected)\n");
  LetterboxParams sq = compute_letterbox(640, 640, 320, 320);
  assert(sq.pad_x == 0 && sq.pad_y == 0);
  assert(sq.new_w == 320 && sq.new_h == 320);
  roundtrip(640, 640, 320, 320, 10, 10, 630, 630);

  printf("identity scale (scale == 1.0, lround overshoot guard)\n");
  LetterboxParams id = compute_letterbox(320, 320, 320, 320);
  assert(id.scale == 1.0f && id.new_w == 320 && id.new_h == 320);
  assert(id.pad_x == 0 && id.pad_y == 0);

  printf("odd dimensions\n");
  roundtrip(1279, 721, 320, 320, 3, 7, 1200, 700);

  printf("upscale (small source, large canvas)\n");
  roundtrip(160, 90, 640, 640, 10, 5, 150, 85);

  printf("clamping: box extending into padding is clipped to image\n");
  LetterboxParams p = compute_letterbox(1280, 720, 320, 320);
  BoxPx b = undo_letterbox(p, 0.0f, 0.0f, 1.0f, 1.0f);
  printf("    full-canvas box -> (%.1f,%.1f,%.1f,%.1f)\n", b.x0, b.y0, b.x1, b.y1);
  assert(b.x0 >= 0.0f && b.y0 >= 0.0f);
  assert(b.x1 <= 1280.0f && b.y1 <= 720.0f);
  // The padding is on the vertical axis here, so a full-canvas box should
  // clamp to exactly the source height.
  assert(close(b.y0, 0.0f) && close(b.y1, 720.0f));

  printf("\nall letterbox tests passed\n");
  return 0;
}
