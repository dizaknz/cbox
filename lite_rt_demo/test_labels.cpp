#include "labels.h"
#include <cassert>
#include <cstdio>

static void write(const std::string& p, const std::string& s) {
  std::ofstream f(p); f << s;
}

int main() {
  // (a) plain COCO-80 style
  write("/tmp/a.txt", "person\nbicycle\ncar\n");
  LabelMap a; assert(a.Load("/tmp/a.txt"));
  printf("a: %s -> cls0=%s cls2=%s\n", a.Describe().c_str(),
         a.Get(0).c_str(), a.Get(2).c_str());
  assert(a.Get(0) == "person" && a.Get(2) == "car");

  // (c) leading background -> offset should become 1
  write("/tmp/c.txt", "background\nperson\nbicycle\ncar\n");
  LabelMap c; assert(c.Load("/tmp/c.txt"));
  printf("c: %s -> cls0=%s cls2=%s\n", c.Describe().c_str(),
         c.Get(0).c_str(), c.Get(2).c_str());
  assert(c.offset() == 1 && c.Get(0) == "person" && c.Get(2) == "car");

  // (b) "???" placeholders mid-file fall through to the numeric fallback
  write("/tmp/b.txt", "person\nbicycle\n???\ncar\n");
  LabelMap b; assert(b.Load("/tmp/b.txt"));
  printf("b: %s -> cls2=%s cls3=%s\n", b.Describe().c_str(),
         b.Get(2).c_str(), b.Get(3).c_str());
  assert(b.Get(2) == "class 2" && b.Get(3) == "car");

  // (d) indexed pairs, sparse
  write("/tmp/d.txt", "0  person\n1  bicycle\n5  bus\n");
  LabelMap d; assert(d.Load("/tmp/d.txt"));
  printf("d: %s -> cls0=%s cls5=%s cls3=%s\n", d.Describe().c_str(),
         d.Get(0).c_str(), d.Get(5).c_str(), d.Get(3).c_str());
  assert(d.Get(0) == "person" && d.Get(5) == "bus" && d.Get(3) == "class 3");

  // labels containing digits must not trip the indexed heuristic
  write("/tmp/e.txt", "person\n3d glasses\ncar\n");
  LabelMap e; assert(e.Load("/tmp/e.txt"));
  printf("e: %s -> cls1=%s\n", e.Describe().c_str(), e.Get(1).c_str());
  assert(e.Get(1) == "3d glasses");

  // CRLF + trailing blank line
  write("/tmp/f.txt", "person\r\nbicycle\r\ncar\r\n\n");
  LabelMap f; assert(f.Load("/tmp/f.txt"));
  printf("f: %s -> cls1=%s size=%zu\n", f.Describe().c_str(),
         f.Get(1).c_str(), f.size());
  assert(f.Get(1) == "bicycle" && f.size() == 3);

  // missing file
  LabelMap g; std::string err;
  assert(!g.Load("/tmp/does_not_exist.txt", &err));
  printf("g: %s\n", err.c_str());

  printf("\nall label tests passed\n");
}
