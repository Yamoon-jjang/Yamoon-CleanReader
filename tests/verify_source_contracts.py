#!/usr/bin/env python3
"""Build-time checks for the current release (v2.4.5).

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
assert 'kVersion = L"2.4.5"' in main
assert "FILEVERSION 2,4,5,0" in rc and 'VALUE "ProductVersion", "2.4.5"' in rc
assert "야문 클린리더 v2.4.5" in manual
assert "v2.4.4" in manual and "16. v2.4.5" in manual
# v2.4.5 reading-mode TOC: old rule kept as-is, new rule only adds headings
assert "static bool IsChapterLineBase(" in main and "static bool IsChapterLineMore(" in main
assert "return IsChapterLineBase(t, a, b) || IsChapterLineMore(t, a, b);" in main
assert "TocHanNum(" in main
# v2.4.4 usability contracts
assert "[위치 확인]" not in main and "위치 확인]" not in manual.split(" 13. v2.4.3")[0]
assert "missing[k] ? MF_GRAYED" in main                      # 옮겨지거나 지워진 최근 파일은 회색
assert "if (c == L'&') o += L\"&&\";" in main               # 메뉴 제목의 & 를 그대로 표시
assert "ch = L'+'" not in main
assert main.count("SwallowMenuReclick((HWND)lp)") == 4       # 목차·책갈피·최근·테마 단추 다시 누르면 닫힘
assert "TrackPopupMenu(m, flags | TPM_RETURNCMD" in main
assert "RpPlaceCenter(rw)" in search and "if (g_rpWnd) RpPlaceCenter(g_rpWnd)" in main
assert "id == ID_RP_NEXT || id == ID_RL_OK" not in main      # [찾기] 단추는 색 채움 없음
assert "u16:" in main and "RecoverDamagedRecentPath" in main
assert "kMixQuoteSpan" in engine  # do not alter long-standing text rules
# v2.4.4 same-version redeploy: every match highlighted (visible range only), encodings
assert "static void RpMarkTick()" in search and "SetTimer(h, 8, 60, nullptr)" in search and "KillTimer(h, 8)" in search
assert "doc->Undo(tomSuspend, nullptr)" in search and "tomAutoColor" in search   # not in undo, back to automatic
assert search.count("RpMarkClear();") >= 4                                      # before every text change + on close
# [찾기]로 다음 찾은 곳: 선택 표시를 감춘 채로는 RichEdit가 화면을 옮기지 않으므로 되살리고 직접 옮김
assert "static void RpGoHit(const RpHit& hh)" in search and search.count("RpGoHit(hits[pick])") == 3
go = search[search.index("static void RpGoHit("):]
go = go[:go.index("\n}\n")]
assert go.index("RpShowSel(true)") < go.index("EM_SETSEL") and "RpReveal(" in go and "RpMarkTick()" in go
assert "SendMessageW(g_dst, EM_SETSEL, hh.a, hh.b);\n    SendMessageW(g_dst, EM_SCROLLCARET" not in search
assert "E_EUCJP" in main and "case E_GBK: return 54936;" in main and "readenc2" in main
assert "일본어 (EUC-JP)" in main and "EUC-JP" in manual and "GB18030" in manual and "모두 바탕색으로" in manual

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
print("v2.4.5 source contracts and 600,001 actual hit-index cases: PASS")
