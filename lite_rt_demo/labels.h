// labels.h -- loader for TFLite detection label maps.
//
// Label files in the wild come in at least four shapes:
//
//   (a) one label per line, index == line number, 80 entries (COCO-80)
//   (b) one label per line, 90 entries with "???" placeholders where COCO
//       category IDs were retired (COCO-90). EfficientDet-Lite and the TF1
//       SSD exports use this.
//   (c) same as above but with a leading "background" / "???" line, so the
//       real classes start at index 1
//   (d) "<index><whitespace><label>" pairs, possibly sparse
//
// Guessing wrong shifts every label by one or two, which looks like a broken
// model. This loader handles all four and reports what it decided.

#pragma once

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class LabelMap {
 public:
  bool Load(const std::string& path, std::string* err = nullptr) {
    std::ifstream f(path);
    if (!f) {
      if (err) *err = "cannot open label file: " + path;
      return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();
      lines.push_back(line);
    }
    if (lines.empty()) {
      if (err) *err = "label file is empty: " + path;
      return false;
    }

    // Drop trailing blank lines 
    while (!lines.empty() && Trim(lines.back()).empty()) lines.pop_back();

    if (LooksIndexed(lines)) {
      LoadIndexed(lines);
      format_ = "indexed (\"<id> <label>\")";
    } else {
      LoadPerLine(lines);
      format_ = "one label per line";
    }
    return true;
  }

  // Returns the label for a model class index, or a "class N" fallback so a
  // mismatch is visible rather than silently blank.
  std::string Get(int cls) const {
    auto it = map_.find(cls + offset_);
    if (it != map_.end() && !it->second.empty() && it->second != "???")
      return it->second;
    return "class " + std::to_string(cls);
  }

  // Some models emit class 0 as the first real category, others reserve 0 for
  // background. If your labels are consistently off by one, flip this.
  void SetOffset(int offset) { offset_ = offset; }
  int offset() const { return offset_; }

  size_t size() const { return map_.size(); }
  const std::string& format() const { return format_; }

  // What the loader inferred, for logging at startup.
  std::string Describe() const {
    std::ostringstream os;
    os << size() << " labels, " << format() << ", offset=" << offset_;
    if (has_leading_placeholder_)
      os << " (leading placeholder detected)";
    return os.str();
  }

 private:
  static std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
  }

  // Treat the file as indexed only if a clear majority of non-blank lines
  // start with digits followed by whitespace. A single label that happens to
  // begin with a digit should not flip the decision.
  static bool LooksIndexed(const std::vector<std::string>& lines) {
    int indexed = 0, total = 0;
    for (const auto& raw : lines) {
      std::string s = Trim(raw);
      if (s.empty()) continue;
      ++total;
      size_t i = 0;
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
      if (i > 0 && i < s.size() &&
          std::isspace(static_cast<unsigned char>(s[i]))) {
        ++indexed;
      }
    }
    return total > 0 && indexed * 2 > total;
  }

  void LoadIndexed(const std::vector<std::string>& lines) {
    for (const auto& raw : lines) {
      std::string s = Trim(raw);
      if (s.empty()) continue;
      std::istringstream is(s);
      int idx;
      if (!(is >> idx)) continue;
      std::string rest;
      std::getline(is, rest);
      map_[idx] = Trim(rest);
    }
  }

  void LoadPerLine(const std::vector<std::string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i)
      map_[static_cast<int>(i)] = Trim(lines[i]);

    // A leading "background" or "???" means real classes start at index 1,
    // so a model emitting class 0 for "person" needs +1.
    std::string first = map_.count(0) ? map_.at(0) : "";
    std::string lower;
    for (char c : first) lower += static_cast<char>(std::tolower(
                                     static_cast<unsigned char>(c)));
    if (lower == "background" || lower == "???" || lower.empty()) {
      has_leading_placeholder_ = true;
      offset_ = 1;
    }
  }

  std::unordered_map<int, std::string> map_;
  std::string format_;
  int offset_ = 0;
  bool has_leading_placeholder_ = false;
};
