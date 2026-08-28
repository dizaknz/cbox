#include "detector.h"

#include <cmath>
#include <cstring>
#include <sstream>

#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"

namespace {

int NumElements(const TfLiteTensor* t) {
  int n = 1;
  for (int i = 0; i < t->dims->size; ++i) n *= t->dims->data[i];
  return n;
}

std::string ShapeString(const TfLiteTensor* t) {
  std::ostringstream os;
  os << "[";
  for (int i = 0; i < t->dims->size; ++i) {
    if (i) os << ",";
    os << t->dims->data[i];
  }
  os << "]";
  return os.str();
}

}  // namespace

bool Detector::Init(const std::string& model_path, const DetectorOptions& opts,
                    std::string* err) {
  opts_ = opts;

  model_ = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
  if (!model_) {
    if (err) *err = "failed to load model: " + model_path;
    return false;
  }

  tflite::ops::builtin::BuiltinOpResolver resolver;
  if (tflite::InterpreterBuilder(*model_, resolver)(&interp_) != kTfLiteOk ||
      !interp_) {
    if (err) *err = "failed to build interpreter";
    return false;
  }

  interp_->SetNumThreads(opts_.num_threads);

  // The delegate must be applied before AllocateTensors, otherwise the graph
  // is already partitioned and the delegate silently does nothing.
  if (opts_.use_xnnpack) {
    auto xopts = TfLiteXNNPackDelegateOptionsDefault();
    xopts.num_threads = opts_.num_threads;
    xnnpack_ = TfLiteXNNPackDelegateCreate(&xopts);
    if (xnnpack_ &&
        interp_->ModifyGraphWithDelegate(xnnpack_) != kTfLiteOk) {
      // Not fatal: fully-int8 graphs often fall back to the reference kernels.
      TfLiteXNNPackDelegateDelete(xnnpack_);
      xnnpack_ = nullptr;
    }
  }

  if (interp_->AllocateTensors() != kTfLiteOk) {
    if (err) *err = "AllocateTensors failed";
    return false;
  }

  const TfLiteTensor* in = interp_->input_tensor(0);
  if (in->dims->size != 4) {
    if (err) *err = "expected a 4-D input tensor, got " + ShapeString(in);
    return false;
  }
  in_h_ = in->dims->data[1];
  in_w_ = in->dims->data[2];
  quantized_ = (in->type == kTfLiteUInt8 || in->type == kTfLiteInt8);
  in_scale_ = in->params.scale;
  in_zero_point_ = in->params.zero_point;

  if (!DiscoverOutputsByShape(err)) return false;

  if (!opts_.label_path.empty()) {
    std::string lerr;
    have_labels_ = labels_.Load(opts_.label_path, &lerr);
    if (!have_labels_ && err) *err = lerr;  // non-fatal; caller may ignore
  }
  return true;
}

// Identify outputs by shape rather than by index, because the converter is
// free to permute them and different exports of the same architecture do.
//   boxes -> rank 3, last dim 4     ([1, N, 4])
//   count -> a single element       ([1] or scalar)
//   scores/classes -> rank 2        ([1, N]) -- indistinguishable by shape,
//                                   so they are resolved on the first frame.
bool Detector::DiscoverOutputsByShape(std::string* err) {
  std::vector<int> rank2;
  std::ostringstream note;

  for (size_t i = 0; i < interp_->outputs().size(); ++i) {
    const TfLiteTensor* t = interp_->output_tensor(i);
    const int idx = static_cast<int>(i);
    note << "  out[" << i << "] " << ShapeString(t)
         << " name=" << (t->name ? t->name : "(null)") << "\n";

    if (t->dims->size == 3 && t->dims->data[2] == 4) {
      idx_boxes_ = idx;
      num_slots_ = t->dims->data[1];
    } else if (NumElements(t) == 1) {
      idx_count_ = idx;
    } else if (t->dims->size == 2) {
      rank2.push_back(idx);
    }
  }

  if (idx_boxes_ < 0) {
    if (err) {
      *err = "no [1,N,4] box tensor found -- this does not look like a model "
             "with a built-in TFLite_Detection_PostProcess. Raw YOLO exports "
             "need their own decode + NMS.\n" + note.str();
    }
    return false;
  }
  if (rank2.size() != 2) {
    if (err) {
      *err = "expected exactly two [1,N] outputs (scores, classes), found " +
             std::to_string(rank2.size()) + "\n" + note.str();
    }
    return false;
  }

  // Provisional assignment; corrected on the first real inference.
  idx_classes_ = rank2[0];
  idx_scores_  = rank2[1];
  discovery_note_ = note.str();
  return true;
}

// Scores live in [0,1] and come out sorted descending. Class indices are
// whole numbers and, for any real COCO model, will exceed 1 somewhere in the
// list. Check both signals so a frame containing only class 0 or 1 does not
// flip the assignment.
void Detector::DisambiguateScoresAndClasses() {
  const float* a = interp_->typed_output_tensor<float>(idx_classes_);
  const float* b = interp_->typed_output_tensor<float>(idx_scores_);
  const int n = num_slots_;

  auto all_integral = [&](const float* v) {
    for (int i = 0; i < n; ++i)
      if (std::fabs(v[i] - std::round(v[i])) > 1e-4f) return false;
    return true;
  };
  auto any_above_one = [&](const float* v) {
    for (int i = 0; i < n; ++i) if (v[i] > 1.0f) return true;
    return false;
  };
  auto is_descending = [&](const float* v) {
    for (int i = 1; i < n; ++i) if (v[i] > v[i - 1] + 1e-6f) return false;
    return true;
  };

  bool swap = false;
  if (any_above_one(b) && !any_above_one(a)) {
    swap = true;                       // b holds class ids
  } else if (!any_above_one(a) && !any_above_one(b)) {
    // Both in [0,1]: fall back to ordering and integrality.
    if (is_descending(a) && !is_descending(b)) swap = true;
    else if (all_integral(b) && !all_integral(a)) swap = true;
  }

  if (swap) std::swap(idx_classes_, idx_scores_);
  roles_resolved_ = true;
}

bool Detector::Detect(const cv::Mat& frame, std::vector<Detection>* out,
                      std::string* err) {
  out->clear();
  if (frame.empty()) {
    if (err) *err = "empty frame";
    return false;
  }

  cv::Mat rgb;
  cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

  cv::Mat net_in;
  LetterboxParams lb;
  if (opts_.use_letterbox) {
    lb = apply_letterbox(rgb, net_in, in_w_, in_h_);
  } else {
    cv::resize(rgb, net_in, cv::Size(in_w_, in_h_));
    // A plain stretch is a letterbox with no padding and separate x/y scales;
    // model as full-frame so undo_letterbox reduces to a simple rescale.
    lb = compute_letterbox(frame.cols, frame.rows, in_w_, in_h_);
    lb.scale = 1.0f;  lb.pad_x = 0;  lb.pad_y = 0;
    lb.dst_w = frame.cols;  lb.dst_h = frame.rows;
  }

  const size_t n_px = static_cast<size_t>(in_w_) * in_h_ * 3;
  if (quantized_) {
    // uint8 models are almost always scale=1/255, zero_point=0, i.e. raw
    // pixels. Anything else needs a real requantization, so bail loudly
    // rather than producing silently wrong results.
    if (interp_->input_tensor(0)->type == kTfLiteUInt8) {
      std::memcpy(interp_->typed_input_tensor<uint8_t>(0), net_in.data, n_px);
    } else {  // int8: shift the uint8 range down by 128
      int8_t* dst = interp_->typed_input_tensor<int8_t>(0);
      const uint8_t* src = net_in.data;
      for (size_t i = 0; i < n_px; ++i)
        dst[i] = static_cast<int8_t>(static_cast<int>(src[i]) - 128);
    }
  } else {
    cv::Mat f;
    net_in.convertTo(f, CV_32FC3, 1.0 / 127.5, -1.0);   // [-1, 1]
    std::memcpy(interp_->typed_input_tensor<float>(0), f.data,
                n_px * sizeof(float));
  }

  if (interp_->Invoke() != kTfLiteOk) {
    if (err) *err = "Invoke failed";
    return false;
  }

  if (!roles_resolved_) DisambiguateScoresAndClasses();

  const float* boxes   = interp_->typed_output_tensor<float>(idx_boxes_);
  const float* classes = interp_->typed_output_tensor<float>(idx_classes_);
  const float* scores  = interp_->typed_output_tensor<float>(idx_scores_);

  int n = num_slots_;
  if (idx_count_ >= 0) {
    const float c = *interp_->typed_output_tensor<float>(idx_count_);
    n = std::min(num_slots_, static_cast<int>(std::lround(c)));
  }
  n = std::min(n, opts_.max_detections);

  for (int i = 0; i < n; ++i) {
    if (scores[i] < opts_.score_threshold) continue;

    // TFLite_Detection_PostProcess emits (ymin, xmin, ymax, xmax).
    Detection d;
    d.box = undo_letterbox(lb, boxes[i * 4 + 0], boxes[i * 4 + 1],
                               boxes[i * 4 + 2], boxes[i * 4 + 3]);
    d.score = scores[i];
    d.cls = static_cast<int>(std::lround(classes[i]));
    d.label = have_labels_ ? labels_.Get(d.cls)
                           : ("class " + std::to_string(d.cls));

    // Degenerate boxes appear when a detection sits entirely in the padding.
    if (d.box.x1 - d.box.x0 < 1.0f || d.box.y1 - d.box.y0 < 1.0f) continue;
    out->push_back(std::move(d));
  }
  return true;
}

std::string Detector::Describe() const {
  std::ostringstream os;
  os << "input " << in_w_ << "x" << in_h_
     << (quantized_ ? " quantized" : " float32")
     << ", xnnpack=" << (xnnpack_ ? "on" : "off")
     << ", threads=" << opts_.num_threads
     << ", letterbox=" << (opts_.use_letterbox ? "on" : "off") << "\n"
     << "outputs:\n" << discovery_note_
     << "resolved: boxes=" << idx_boxes_ << " scores=" << idx_scores_
     << " classes=" << idx_classes_ << " count=" << idx_count_
     << (roles_resolved_ ? " (confirmed)" : " (provisional)") << "\n";
  if (have_labels_) os << "labels: " << labels_.Describe() << "\n";
  return os.str();
}
