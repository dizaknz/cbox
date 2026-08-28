#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model_builder.h"

#include "labels.h"
#include "letterbox.h"

struct Detection {
  BoxPx box;        // original-image pixel coordinates
  float score;
  int   cls;
  std::string label;
};

struct DetectorOptions {
  int   num_threads      = 4;
  float score_threshold  = 0.5f;
  int   max_detections   = 25;
  bool  use_xnnpack      = true;
  bool  use_letterbox    = true;   // false = stretch (matches most tutorials)
  std::string label_path;          // optional
};

class Detector {
 public:
  bool Init(const std::string& model_path, const DetectorOptions& opts, std::string* err);

  // Runs one frame. `frame` is BGR (as OpenCV gives you).
  bool Detect(const cv::Mat& frame, std::vector<Detection>* out, std::string* err);

  int input_width()  const { return in_w_; }
  int input_height() const { return in_h_; }
  bool quantized()   const { return quantized_; }

  // Human-readable dump of what the loader inferred. Print this once at
  // startup -- it is the fastest way to catch a model whose outputs are
  // ordered differently from what you assumed.
  std::string Describe() const;

 private:
  bool DiscoverOutputsByShape(std::string* err);
  void DisambiguateScoresAndClasses();

  std::unique_ptr<tflite::FlatBufferModel> model_;
  std::unique_ptr<tflite::Interpreter> interp_;
  TfLiteDelegate* xnnpack_ = nullptr;

  DetectorOptions opts_;
  LabelMap labels_;
  bool have_labels_ = false;

  int in_w_ = 0, in_h_ = 0;
  bool quantized_ = false;
  float in_scale_ = 0.0f;      // quantization params, when quantized
  int   in_zero_point_ = 0;

  // Resolved output tensor indices.
  int idx_boxes_ = -1, idx_classes_ = -1, idx_scores_ = -1, idx_count_ = -1;
  int num_slots_ = 0;          // N in [1, N, 4]
  bool roles_resolved_ = false;  // scores/classes disambiguated yet?
  std::string discovery_note_;
};
