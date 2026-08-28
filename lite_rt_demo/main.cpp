// Camera loop: grab -> detect -> draw, with a rolling FPS readout.
//
// Camera source notes for Raspberry Pi OS Bookworm and later:
//
//   USB webcam        -> V4L2 works directly:  --source=0
//   CSI ribbon camera -> the legacy V4L2 shim is gone; libcamera is the only
//                        path. Either run the binary under `libcamerify`, or
//                        pass the GStreamer pipeline below (needs OpenCV
//                        built with GStreamer support -- check
//                        cv::getBuildInformation() for "GStreamer: YES").
//   Video file        -> --source=clip.mp4
//
// CSI pipeline:
//   --source="libcamerasrc ! video/x-raw,width=1280,height=720,framerate=30/1 \
//             ! videoconvert ! appsink drop=true max-buffers=2"

#include <chrono>
#include <cstdio>
#include <deque>
#include <string>

#include <opencv2/opencv.hpp>

#include "detector.h"

namespace {

// A pipeline string is anything that is not a bare integer and contains a '!'.
bool LooksLikeGstPipeline(const std::string& s) {
  return s.find('!') != std::string::npos;
}

bool IsInteger(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  return true;
}

cv::Scalar ColorForClass(int cls) {
  // Deterministic, well-spread hues so the same class keeps its colour.
  const int hue = (cls * 47) % 180;
  cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, 200, 255)), bgr;
  cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
  cv::Vec3b p = bgr.at<cv::Vec3b>(0, 0);
  return cv::Scalar(p[0], p[1], p[2]);
}

void DrawDetection(cv::Mat& img, const Detection& d) {
  const cv::Scalar color = ColorForClass(d.cls);
  const cv::Point p0(static_cast<int>(d.box.x0), static_cast<int>(d.box.y0));
  const cv::Point p1(static_cast<int>(d.box.x1), static_cast<int>(d.box.y1));
  cv::rectangle(img, p0, p1, color, 2);

  char text[128];
  std::snprintf(text, sizeof(text), "%s %.0f%%", d.label.c_str(), d.score * 100);

  int baseline = 0;
  const cv::Size ts =
      cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

  // Put the label inside the box when it would otherwise run off the top.
  const int label_y = (p0.y - ts.height - 4 < 0) ? p0.y + ts.height + 4 : p0.y;
  cv::rectangle(img, {p0.x, label_y - ts.height - 4},
                {p0.x + ts.width + 4, label_y}, color, cv::FILLED);
  cv::putText(img, text, {p0.x + 2, label_y - 3}, cv::FONT_HERSHEY_SIMPLEX,
              0.5, {0, 0, 0}, 1, cv::LINE_AA);
}

std::string ArgValue(int argc, char** argv, const std::string& key,
                     const std::string& fallback) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a.rfind(key + "=", 0) == 0) return a.substr(key.size() + 1);
  }
  return fallback;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string model  = ArgValue(argc, argv, "--model", "");
  const std::string labels = ArgValue(argc, argv, "--labels", "");
  const std::string source = ArgValue(argc, argv, "--source", "0");
  const bool headless      = ArgValue(argc, argv, "--headless", "0") == "1";

  if (model.empty()) {
    std::fprintf(stderr,
                 "usage: %s --model=det.tflite [--labels=coco.txt] "
                 "[--source=0|file|gst-pipeline] [--headless=1]\n", argv[0]);
    return 1;
  }

  DetectorOptions opts;
  opts.label_path = labels;
  opts.num_threads = 4;
  opts.score_threshold = 0.5f;

  Detector det;
  std::string err;
  if (!det.Init(model, opts, &err)) {
    std::fprintf(stderr, "init failed: %s\n", err.c_str());
    return 1;
  }
  std::fputs(det.Describe().c_str(), stdout);

  cv::VideoCapture cap;
  if (IsInteger(source)) {
    cap.open(std::stoi(source), cv::CAP_V4L2);
    // MJPG keeps USB bandwidth manageable at 720p+; without it many webcams
    // silently drop to 5 fps on raw YUYV.
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    // Shrink the driver queue so we process fresh frames, not a backlog.
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
  } else if (LooksLikeGstPipeline(source)) {
    cap.open(source, cv::CAP_GSTREAMER);
  } else {
    cap.open(source);
  }

  if (!cap.isOpened()) {
    std::fprintf(stderr, "cannot open source: %s\n", source.c_str());
    return 1;
  }

  std::deque<double> recent;   // rolling window of per-frame inference times
  cv::Mat frame;
  std::vector<Detection> dets;

  while (true) {
    if (!cap.read(frame) || frame.empty()) break;

    const auto t0 = std::chrono::steady_clock::now();
    if (!det.Detect(frame, &dets, &err)) {
      std::fprintf(stderr, "detect failed: %s\n", err.c_str());
      break;
    }
    const auto t1 = std::chrono::steady_clock::now();

    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    recent.push_back(ms);
    if (recent.size() > 30) recent.pop_front();
    double avg = 0;
    for (double v : recent) avg += v;
    avg /= static_cast<double>(recent.size());

    for (const auto& d : dets) DrawDetection(frame, d);

    char hud[96];
    std::snprintf(hud, sizeof(hud), "%.1f ms  %.1f fps  %zu objects",
                  avg, 1000.0 / avg, dets.size());
    cv::putText(frame, hud, {10, 25}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                {0, 255, 0}, 2, cv::LINE_AA);

    if (headless) {
      std::printf("%s\n", hud);
      for (const auto& d : dets)
        std::printf("  %-16s %.2f  [%.0f,%.0f,%.0f,%.0f]\n", d.label.c_str(),
                    d.score, d.box.x0, d.box.y0, d.box.x1, d.box.y1);
    } else {
      cv::imshow("detections", frame);
      const int key = cv::waitKey(1) & 0xFF;
      if (key == 27 || key == 'q') break;   // Esc or q
    }
  }
  return 0;
}
