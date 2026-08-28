// letterbox.h -- aspect-preserving resize + inverse coordinate mapping.
//
// The geometry is deliberately free of any OpenCV dependency so it can be
// unit tested on its own. apply_letterbox() is the only part that touches
// cv::Mat, and it lives behind DETECT_WITH_OPENCV.

#pragma once

#include <algorithm>
#include <cmath>

struct LetterboxParams {
  float scale;   // multiply source pixels by this to reach letterboxed space
  int   new_w;   // size of the scaled image inside the canvas
  int   new_h;
  int   pad_x;   // left padding (canvas is dst_w x dst_h)
  int   pad_y;   // top padding
  int   dst_w;
  int   dst_h;
  int   src_w;
  int   src_h;
};

// Fit a src_w x src_h image into a dst_w x dst_h canvas without distortion,
// centering it and leaving symmetric padding on the short axis.
inline LetterboxParams compute_letterbox(int src_w, int src_h,
                                         int dst_w, int dst_h) {
  LetterboxParams p{};
  p.src_w = src_w;  p.src_h = src_h;
  p.dst_w = dst_w;  p.dst_h = dst_h;

  p.scale = std::min(static_cast<float>(dst_w) / static_cast<float>(src_w),
                     static_cast<float>(dst_h) / static_cast<float>(src_h));

  // round() rather than truncation: with truncation a 1279x720 -> 640x640
  // fit loses a pixel column and the inverse mapping drifts.
  p.new_w = static_cast<int>(std::lround(src_w * p.scale));
  p.new_h = static_cast<int>(std::lround(src_h * p.scale));

  // Clamp: lround can overshoot by one when scale is exactly 1.0.
  p.new_w = std::min(p.new_w, dst_w);
  p.new_h = std::min(p.new_h, dst_h);

  p.pad_x = (dst_w - p.new_w) / 2;
  p.pad_y = (dst_h - p.new_h) / 2;
  return p;
}

// A box in original-image pixel coordinates.
struct BoxPx { float x0, y0, x1, y1; };

// Undo the letterbox. Input is a normalized box as emitted by
// TFLite_Detection_PostProcess: values in [0,1] relative to the *letterboxed*
// canvas, in (ymin, xmin, ymax, xmax) order.
inline BoxPx undo_letterbox(const LetterboxParams& p,
                            float ymin, float xmin, float ymax, float xmax) {
  // normalized -> canvas pixels
  float cx0 = xmin * p.dst_w, cy0 = ymin * p.dst_h;
  float cx1 = xmax * p.dst_w, cy1 = ymax * p.dst_h;

  // canvas pixels -> source pixels
  BoxPx b;
  b.x0 = (cx0 - p.pad_x) / p.scale;
  b.y0 = (cy0 - p.pad_y) / p.scale;
  b.x1 = (cx1 - p.pad_x) / p.scale;
  b.y1 = (cy1 - p.pad_y) / p.scale;

  // A detection can legitimately extend into the padding; clamp to the image.
  b.x0 = std::clamp(b.x0, 0.0f, static_cast<float>(p.src_w));
  b.y0 = std::clamp(b.y0, 0.0f, static_cast<float>(p.src_h));
  b.x1 = std::clamp(b.x1, 0.0f, static_cast<float>(p.src_w));
  b.y1 = std::clamp(b.y1, 0.0f, static_cast<float>(p.src_h));
  return b;
}

#ifdef DETECT_WITH_OPENCV
#include <opencv2/opencv.hpp>

// Resize `src` into a dst_w x dst_h canvas, preserving aspect ratio.
// Pad value 114 is the YOLO/Ultralytics convention and is a reasonable
// neutral for SSD/EfficientDet too -- it is far from any saturated colour,
// so the padding does not read as a strong edge to the first conv layer.
inline LetterboxParams apply_letterbox(const cv::Mat& src, cv::Mat& dst,
                                       int dst_w, int dst_h,
                                       const cv::Scalar& pad = {114, 114, 114}) {
  LetterboxParams p = compute_letterbox(src.cols, src.rows, dst_w, dst_h);

  cv::Mat scaled;
  // INTER_AREA when shrinking (the usual case) avoids aliasing that
  // INTER_LINEAR introduces on large downscales.
  const int interp = (p.scale < 1.0f) ? cv::INTER_AREA : cv::INTER_LINEAR;
  cv::resize(src, scaled, cv::Size(p.new_w, p.new_h), 0, 0, interp);

  dst.create(dst_h, dst_w, src.type());
  dst.setTo(pad);
  scaled.copyTo(dst(cv::Rect(p.pad_x, p.pad_y, p.new_w, p.new_h)));
  return p;
}
#endif  // DETECT_WITH_OPENCV
