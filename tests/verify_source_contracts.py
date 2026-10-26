#!/usr/bin/env python3
"""Build-time checks for the v2.4.3 optimization candidate.

Deliberately do not reimplement the document-cleanup logic here. We enforce
source-module and version contracts, and compile/run the actual hit-index
helper extracted from the Win32 search module on the host toolchain.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.cpp").read_text(encoding="utf-8")
search = (root / "src/search_replace.inc").read_text(encoding="utf-8")
engine = (root / "src/engine.cpp").read_text(encoding="utf-8")
head = (root / "src/engine.h").read_text(encoding="utf-8")
rc = (root / "src/app.rc").read_text(encoding="utf-8")
manual = (root / "dist/README.txt").read_text(encoding="utf-8-sig")
assert '#include "search_replace.inc"' in main
assert "static size_t RpHitFrom(" in search
assert search.count("RpHitFrom(") >= 5  # helper + consumers
assert "static bool RpSearch(" not in main  # only one implementation
assert "static bool RpSearch(" in search
assert "void dialogueSep" not in head and "dialogueSep" not in head + main + search + engine
assert 'kVersion = L"2.4.3"' in main
assert "FILEVERSION 2,4,3,0" in rc and 'VALUE "ProductVersion", "2.4.3"' in rc
assert "야문 클린리더 v2.4.3" in manual
assert "v2.4.3 시험 빌드" in manual
assert "kMixQuoteSpan" in engine  # do not alter long-standing text rules

# Confirm the engine still has the exact pre-optimization blob, not just a
# superficially similar output for a few sample novels.
actual = subprocess.check_output(["git", "hash-object", "src/engine.cpp"], cwd=root, text=True).strip()
assert actual == "3f876899eaa3e017e09d2d293c61bc63bf13fae3", actual

name = "static size_t RpHitFrom("
start = search.index(name)
opening = search.index("{", start)
depth = 1
end = opening + 1
while depth and end < len(search):
    depth += (search[end] == "{") - (search[end] == "}")
    end += 1
assert depth == 0
func = search[start:end]
harness = r"""
#include <algorithm>
#include <vector>
#include <string>
#include <cstddef>
#include <cassert>
struct RpHit { size_t a, b; std::wstring rep; };
""" + func + r"""
int main() {
  std::vector<RpHit> hits;
  assert(RpHitFrom(hits, 0) == 0);
  for (size_t i = 0; i < 200000; ++i) hits.push_back({3*i+1, 3*i+2, {}});
  for (size_t i = 0; i <= 600000; ++i) {
    const size_t expect = i == 0 ? 0 : std::min((i + 1) / 3, (size_t)200000);
    assert(RpHitFrom(hits, i) == expect);
  }
  for (size_t i = 1; i < hits.size(); i += 137) {
    size_t idx = RpHitFrom(hits, hits[i].a);
    assert(idx == i && hits[idx].b == hits[i].b);
    size_t prior = RpHitFrom(hits, hits[i].a) - 1;
    assert(prior == i - 1);
  }
}
"""
with tempfile.TemporaryDirectory() as td:
    cc = Path(td) / "check.cpp"
    exe = Path(td) / "check"
    cc.write_text(harness, encoding="utf-8")
    subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", str(cc), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("v2.4.3 source contracts and 600,001 actual hit-index cases: PASS")
