#!/usr/bin/env python3
"""Build-time checks for the current release (v2.5.0).

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
assert 'kVersion = L"2.5.0"' in main
assert "FILEVERSION 2,5,0,0" in rc and 'VALUE "ProductVersion", "2.5.0"' in rc
assert "야문 클린리더 v2.5.0" in manual
assert "20. v2.5.0" in manual and manual.index("19. v2.4.8") < manual.index("20. v2.5.0")
assert "17. v2.4.6" in manual and "18. v2.4.7" in manual and manual.index("17. v2.4.6") < manual.index("18. v2.4.7")
assert "19. v2.4.8" in manual and manual.index("18. v2.4.7") < manual.index("19. v2.4.8")
# v2.4.8: 본문 사이 목록·표 줄은 잇지 않고, 문장 끝 이모지는 앞 문장에 붙임 (정리 결과는 tests/run_cases.py가 확인)
assert "bool isListItemStart(const S& t)" in engine and "bool isTableRow(const S& t)" in engine and "bool isEmojiC(C c)" in engine
assert "if (isTableRow(a.t) || isTableRow(b.t)) return false;" in engine
assert "|| listLike || isTableRow(p.x.t)) { outP.push_back" in engine
assert (root / "tests/run_cases.py").exists() and (root / "tests/tidy_cli.cpp").exists()
# v2.4.7: 한국어 긴 대사 기준 120자 + 짧은 문장 묶기 (다른 언어는 200자 그대로)
assert "kLongSpeech = 200;" in engine and "kLongSpeechKo = 120;" in engine
assert "kShortPiece = 10;" in engine and "kPackMax = 30;" in engine and "kTinyPiece = 4;" in engine
assert "if (!ko) return merged;" in engine
assert "120자 이상이면서 3문장 이상" in manual
assert "v2.4.4" in manual and "16. v2.4.5" in manual and "17. v2.4.6" in manual
assert manual.index("16. v2.4.5") < manual.index("17. v2.4.6")

# v2.4.6 flow fixes
def body(name, src=main):
    start = src.index(name)
    brace = src.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (src[end] == "{") - (src[end] == "}")
        end += 1
    return src[start:end]
# unsaved result edits: asked before every way of replacing or closing the text
leave = body("static bool ConfirmLeave(")
assert "HasUnsavedEdits()" in leave and "MB_YESNOCANCEL" in leave and "return DoSave();" in leave
assert main.count("ConfirmLeave(L\"다른 파일을 엶\")") == 4      # 열기 · 최근 파일 · 끌어다 놓기 · 넘겨받기
for action in ("닫음", "비움", "붙여넣은 글로 바꿈", "합친 글을 엶"):
    assert f'ConfirmLeave(L"{action}")' in main, action
close = main[main.index("    case WM_CLOSE:\n        if (g_bwWnd"):]
close = close[:close.index("return 0;\n    case WM_QUERYENDSESSION")]
assert close.index('ConfirmLeave(L"닫음")') < close.index("SaveReadPos();")
unsaved = body("static bool HasUnsavedEdits() {")
assert "g_dstDirty" in unsaved and "g_savedHash" in unsaved and "g_busy" in unsaved
assert "g_savedHash = TextHash(w); g_savedValid = true;" in body("static int SaveTo(")
assert main.count("ForgetSaved();") >= 4                          # 새 글을 열 때마다 저장 기록을 잊음
assert "if (!HasUnsavedEdits()) return true;" in body("static bool ConfirmDiscardEdits()")
src_timer = main[main.index("} else if (wp == TIMER_SRC) {"):]
src_timer = src_timer[:src_timer.index("Reprocess();")]
assert "HasUnsavedEdits()" in src_timer and "g_srcHoldHash" in src_timer
# Windows shutdown: position and settings saved, unsaved edits block with a reason
q = main[main.index("    case WM_QUERYENDSESSION:"):main.index("    case WM_APP_ENDSAVE:")]
assert "SaveReadPos();" in q and "SaveSettings();" in q and "ShutdownBlockReasonCreate" in q and "return FALSE;" in q
assert "ShutdownBlockReasonDestroy(h);" in main
assert "if (g_fullscreen) wp = g_wpBeforeFull;" in body("static void SaveSettings()")
# safe writes and complete reads
wa = body("static bool WriteAll(")
assert "MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)" in wa
assert wa.index("FlushFileBuffers") < wa.index("MoveFileExW") and "DeleteFileW(tmp.c_str());" in wa
assert "if (got != out.size()) { out.clear(); return false; }" in body("static bool ReadAll(")
# save safeguards
sa = body("static bool DoSaveAs()")
assert "IsEpubPath(target)" in sa and "SamePath(target, g_path)" in sa and "MB_DEFBUTTON2" in sa
assert "BusySaveNote()" in body("static bool DoSave()") and "return DoSaveAs();" in body("static bool DoSave()")
# batch: distinct names inside one run, no '?' in results
assert "BwUniqueOuts(j);" in main and main.index("BwUniqueOuts(j);") < main.index("for (auto& o : j->outs) if (FileExists(o)) exist++;")
bt = body("static DWORD WINAPI BwThread(")
assert "if (lossy) ob = Encode(rw, E_UTF8BOM, nullptr);" in bt
# one window
ho = body("static bool HandOffToRunning()")
assert "CreateMutexW" in ho and "ERROR_ALREADY_EXISTS" in ho and "WM_COPYDATA" in ho and "GetFullPathNameW" in ho
assert body("int WINAPI wWinMain(").index("if (HandOffToRunning()) return 0;") < body("int WINAPI wWinMain(").index("RegisterClassExW")
op = main[main.index("    case WM_APP_OPENFILES: {"):main.index("    case WM_DESTROY:\n        PostQuitMessage(0);")]
assert "g_bwWnd || g_rulesWnd || g_aboutWnd || g_busy" in op and 'ConfirmLeave(L"다른 파일을 엶")' in op
# records
assert "PruneMarks();" in body("static void SaveMarks(")
assert "MB_DEFBUTTON2" in main[main.index("if (cmd == ID_RECENT_CLEAR)"):main.index("if (cmd == ID_RECENT_CLEAR)") + 400]
# v2.4.5 reading-mode TOC: old rule kept as-is, new rule only adds headings
assert "static bool IsChapterLineBase(" in main and "static bool IsChapterLineMore(" in main
assert "return IsChapterLineBase(t, a, b) || IsChapterLineMore(t, a, b);" in main
assert "TocHanNum(" in main
# v2.4.4 usability contracts
assert "[위치 확인]" not in main and "위치 확인]" not in manual.split(" 13. v2.4.3")[0]
assert "missing[k] ? MF_GRAYED" in main                      # 옮겨지거나 지워진 최근 파일은 회색
assert "if (c == L'&') o += L\"&&\";" in main               # 메뉴 제목의 & 를 그대로 표시
assert "ch = L'+'" not in main
assert main.count("SwallowMenuReclick((HWND)lp)") == 5       # 목차·책갈피·자동 넘김·최근·테마 단추 다시 누르면 닫힘
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

# v2.5.0: 엔진은 홍보·광고 줄 판정(isPromoLine, "쪽 번호·광고·HTML 제거"를 켰을 때만)만 더함
assert "bool isPromoLine(const S& t)" in engine and "if (isUrlLine(vt) || isPromoLine(vt)) kill = true;" in engine
assert engine.count("isPromoLine(") == 2                     # 정의 + 광고 옵션 안에서만 쓰임
assert (root / "tests/cases/promo_lines_junk/options.txt").read_text().split() == ["--junk"]
# v2.5.0 화면 기능: 계산은 reader_tools.h (tests/check_reader_tools.py가 실제로 컴파일해 시험)
tools = (root / "src/reader_tools.h").read_text(encoding="utf-8")
assert '#include "reader_tools.h"' in main and "inline size_t FixJosa(" in tools and "inline TocCheck CheckTocNumbers(" in tools
assert "rt::FixJosa(vis, e, rep.back(), part)" in search and "at = hh.e; }" in search          # 조사까지 바꿈
assert "!(josa && rt::ParticleAt(vis, e))" in search                                           # 온전한 단어만 + 조사
assert "LVS_OWNERDATA" in search and "RpListRefresh();" in search                              # 찾은 곳 목록 (가상 목록)
assert "rt::CheckTocNumbers(nums, units)" in main                                              # 목차 번호 확인
cp = body("static void ChgPaintPane(")
assert "doc->Undo(tomSuspend, nullptr)" in cp and "tomAutoColor" in cp and "RoUnlock" in cp    # 바뀐 곳 표시: 되돌리기 기록에 남기지 않음
assert "if (k == 1 && (g_dstDirty || g_rpWnd)) return false;" in body("static bool ChgReady(")  # 찾기 창이 열리면 결과 칸은 양보
assert "ChgModeSwitch(on);" in body("static void SetReading(bool on) {") and "if (!on) AutoStop(nullptr);" in body("static void SetReading(bool on) {")
assert "AutoStop(nullptr);" in body("static void ShowSource() {")                                # 새 글을 열면 자동 넘김 멈춤
assert "kColorCustom = 9;" in main and "vcolor3" in main and "v3 = v2 == 8 ? kColorCustom : v2;" in main  # 예전 사용자 지정 배색 이어 쓰기
assert "case WM_XBUTTONUP:\n            return TRUE;" in main
assert "ShellExecuteW(h, L\"open\", kReleasesUrl" in main                                      # 새 버전 확인: 브라우저로 열기만

# Confirm the engine is exactly the reviewed v2.5.0 blob (v2.4.8 + promo-line rule only),
# not just a superficially similar output for a few sample novels.
actual = subprocess.check_output(["git", "hash-object", "src/engine.cpp"], cwd=root, text=True).strip()
assert actual == "2f6fcc4060f67244eb15f3c9fe044c3d4f62de86", actual

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
print("v2.5.0 source contracts (flow fixes) and 600,001 actual hit-index cases: PASS")
