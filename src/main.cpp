// TextTidy - 문단 정리기 (Win32 GUI)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <regex>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <cmath>
#include <shlwapi.h>
#include <shlobj.h>
#include "engine.h"
#include "epub.h"

using std::wstring;
typedef std::u32string U32;

// ---------------------------------------------------------------------------
enum { E_UTF8, E_UTF8BOM, E_CP949, E_UTF16LE, E_UTF16BE, E_SJIS, E_GBK, E_BIG5, E_W1252, E_JOHAB };
static const wchar_t* kEncName[] = {L"UTF-8", L"UTF-8 (BOM)", L"ANSI (CP949)", L"UTF-16 LE", L"UTF-16 BE",
                                    L"일본어 (Shift-JIS)", L"중국어 간체 (GBK)", L"중국어 번체 (Big5)", L"서유럽 (Windows-1252)", L"한글 조합형 (Johab)"};
// 코드 페이지로 읽고 쓰는 인코딩 (유니코드가 아닌 것)
static UINT EncCp(int enc) {
    switch (enc) {
    case E_CP949: return 949; case E_SJIS: return 932; case E_GBK: return 936;
    case E_BIG5: return 950; case E_W1252: return 1252; case E_JOHAB: return 1361;
    default: return 0;
    }
}

enum {
    ID_OPEN = 100, ID_SAVE, ID_SAVEAS, ID_BATCH, ID_HINT,
    ID_BLANK, ID_JOIN, ID_SPACE, ID_SPLIT, ID_LONGLEN, ID_SYNC, ID_PARASEP, ID_INDENT, ID_COLLAPSE, ID_JUNK, ID_ENC,
    ID_QUOTE, ID_ELLIPSIS, ID_HANJA, ID_HALF, ID_READENC, ID_FOLD, ID_ABOUT, ID_ABOUT_OK, ID_SCENE, ID_RESET,
    ID_SRC, ID_DST, ID_PASTE, ID_CLEAR, ID_COPYALL,
    ID_M_CUT, ID_M_COPY, ID_M_PASTE, ID_M_SELALL,
    ID_READ, ID_MODE_TIDY, ID_VRESET, ID_RTOC, ID_RFIND, ID_RMARK, ID_RFULL,
    ID_MARK_ADD, ID_MARK_CLEAR, ID_THEME, ID_RECENT, ID_RECENT_CLEAR, ID_THEME_SYS, ID_THEME_LIGHT, ID_THEME_DARK, ID_SIZE_UP, ID_SIZE_DOWN, ID_VFONT, ID_VSIZE, ID_VCOLOR, ID_VFG, ID_VBG, ID_VLINE, ID_VMARGIN, ID_VBOLD,
    TIMER_LONG = 1, TIMER_SRC = 2, TIMER_SAVED = 3, TIMER_READ = 4,
    ID_TOC_BASE = 20000, ID_MARK_BASE = 24000, ID_RECENT_BASE = 28000  // 목차·책갈피 메뉴 항목
};
enum { ID_RP_FIND = 500, ID_RP_REPL, ID_RP_FINDX, ID_RP_REPLX, ID_RP_CASE, ID_RP_WORD, ID_RP_UP, ID_RP_REGEX, ID_RP_NEXT, ID_RP_REPLACE,
       ID_RP_ALL, ID_RP_COUNT, ID_RP_RULE, ID_RP_LIST, ID_RP_EXPAND, ID_RP_INFO, ID_RP_L1, ID_RP_L2, ID_RP_L3, ID_RP_L4 };
enum { ID_RL_LIST = 540, ID_RL_FIND, ID_RL_REPL, ID_RL_REGEX, ID_RL_LINE, ID_RL_ADD, ID_RL_EDIT, ID_RL_UP, ID_RL_DOWN, ID_RL_DEL,
       ID_RL_PRESET, ID_RL_OK, ID_RL_CANCEL, ID_RL_INFO, ID_RL_L1, ID_RL_L2, ID_RL_HELP };
enum { ID_REPLACE = 600 };
enum {
    ID_BW_LIST = 300, ID_BW_ADD, ID_BW_UP, ID_BW_DOWN, ID_BW_REMOVE, ID_BW_CLEAR, ID_BW_SORT, ID_BW_MODE, ID_BW_DEST,
    ID_BW_START, ID_BW_CLOSE, ID_BW_OPENDIR, ID_BW_PROG
};


static HINSTANCE g_hInst;
static HWND g_hwnd, g_src, g_dst;
static HWND g_btnOpen, g_btnSave, g_btnSaveAs, g_btnBatch, g_btnTheme, g_btnRecent;
static int g_themeMode = 0;  // 0 Windows 설정 따르기, 1 밝게, 2 어둡게
namespace col { extern COLORREF editText, editBg; }
static HWND g_cbBlank, g_cbJoin, g_cbSpace, g_cbSplit, g_edLong, g_chkSync, g_cbParaSep, g_cbIndent, g_chkCollapse, g_chkJunk, g_cbEnc;
static HWND g_btnPaste, g_btnClear, g_btnCopyAll, g_btnReplace;  // 글 칸 머리의 작은 단추
static int g_srcMetaLeft = 0, g_dstMetaLeft = 0;     // 칸 머리 오른쪽 글이 시작할 수 있는 곳
static HWND g_cbScene, g_cbQuote, g_cbEll, g_chkHanja, g_chkHalf, g_cbReadEnc, g_btnFold, g_btnAbout, g_btnReset;
static const wchar_t* kAppName = L"야문 클린리더";
static const wchar_t* kVersion = L"2.4";
static const wchar_t* kDate = L"2026년 10월 19일";
// 빌드 정보: GitHub Actions가 컴파일할 때 -DBUILD_UTC=<UTC 초> -DBUILD_SHA="<커밋>"으로 넣음.
// 직접 빌드하면 값이 없어 정보 창에 "개발본"으로 표시됨.
#ifndef BUILD_UTC
#define BUILD_UTC 0LL
#endif
#ifndef BUILD_SHA
#define BUILD_SHA ""
#endif
static wstring BuildDisplay() {
    const long long t = (long long)(BUILD_UTC);
    if (t <= 0) return L"개발본 (빌드 정보 없음)";
    ULARGE_INTEGER u; u.QuadPart = (ULONGLONG)t * 10000000ULL + 116444736000000000ULL;
    FILETIME ft; ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME utc, loc;
    if (!FileTimeToSystemTime(&ft, &utc)) return L"개발본 (빌드 정보 없음)";
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &loc)) loc = utc;
    wchar_t b[64];
    swprintf(b, 64, L"%04u-%02u-%02u %02u:%02u (현지 시간)", loc.wYear, loc.wMonth, loc.wDay, loc.wHour, loc.wMinute);
    wstring r = b;
    const char* sha = BUILD_SHA;
    if (strlen(sha) >= 8) { r += L" · "; for (int i = 0; i < 8; ++i) r += (wchar_t)sha[i]; }
    return r;
}
// 사용자 규칙 (TextTidy_rules.txt)
static std::vector<std::pair<U32, U32>> g_ruleReplace;
static std::vector<U32> g_ruleDelete;
static std::vector<std::pair<wstring, wstring>> g_ruleRegex;  // 정규식 바꾸기
static std::vector<wstring> g_ruleRegexDel;                   // 정규식 줄 지우기
static wstring g_ruleErrors;                                  // 잘못 쓴 정규식 (목록 저장 때 알려 줌)
static wstring g_rulesPath;
static HFONT g_readFont;  // 읽기 모드 글꼴
static HFONT g_uiFont, g_edFont, g_titleFont, g_cardFont, g_sectFont, g_smallFont, g_dropFont, g_btnFont, g_btnBold, g_aboutTitle;
static wstring g_statusMsg, g_metaSrc, g_metaDst, g_sizeText;  // g_sizeText: 연 파일의 크기 (정리·읽기 칸 머리에 표시)
static wstring SizeText(long long b);  // 파일 크기 글 (예: 68 KB, 3.2 MB)
static std::vector<wstring> g_chips;
static int g_dpi = 96;
static U32 g_text;
static wstring g_path;
static int g_enc = E_UTF8;
static bool g_loading = false, g_dstDirty = false, g_syncing = false, g_srcPending = false;
static U32 g_out;                    // 현재 결과(엔진 출력)
static std::vector<int> g_outToSrc;  // 결과 글자 -> 원본 글자 위치
static std::vector<int> g_srcToOut;  // 원본 글자 -> 결과 글자 위치
static wstring g_ini;

static int S(int v) { return MulDiv(v, g_dpi, 96); }

// ---------------------------------------------------------------------------
// 측정판 (빌드할 때 -DMEASURE를 줄 때만 들어감. 배포판에는 아무것도 들어가지 않음)
// 파일 열기·모드 전환 때 단계마다 걸린 시간을 재서 창으로 보여 주고, 실행 파일 옆 기록 파일에 덧붙임
#define WM_APP_MEAS (WM_APP + 40)
#ifdef MEASURE
namespace meas {
static bool on = false;          // 재는 중
static LARGE_INTEGER qf, t0, tl;
static wstring title;
static std::vector<std::pair<wstring, double>> steps;
static std::vector<wstring> notes;
static wstring logPath;
static double Ms(const LARGE_INTEGER& a, const LARGE_INTEGER& b) { return (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)qf.QuadPart; }
static void Begin(const wstring& t) {
    on = true;
    title = t;
    steps.clear();
    notes.clear();
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&t0);
    tl = t0;
}
static void Mark(const wstring& name) {  // 앞 표시부터 지금까지를 이 단계의 시간으로
    if (!on) return;
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    steps.push_back({name, Ms(tl, n)});
    tl = n;
}
static void Note(const wstring& s) { if (on) notes.push_back(s); }
static wstring Num(long long v) {  // 1,234,567
    wstring s = std::to_wstring(v < 0 ? -v : v), r;
    for (size_t i = 0; i < s.size(); ++i) { if (i && (s.size() - i) % 3 == 0) r += L','; r += s[i]; }
    return v < 0 ? L"-" + r : r;
}
static void Finish();
static void Tick();
static void LogLine(const wstring& s);
static bool mon = false;                  // 첫 화면 뒤 지켜보는 중
static LARGE_INTEGER monStart, monLast, monChange;
static double maxGap = 0;
static int lastMax = -1;
static long long firstFlipMs = -1;        // 열고 나서 첫 쪽 넘김
static bool flipPending = false;
}  // namespace meas
#define MEAS_BEGIN(t) meas::Begin(t)
#define MEAS_MARK(n) meas::Mark(n)
#define MEAS_NOTE(s) meas::Note(s)
#define MEAS_ON() meas::on
#else
#define MEAS_BEGIN(t) ((void)0)
#define MEAS_MARK(n) ((void)0)
#define MEAS_NOTE(s) ((void)0)
#define MEAS_ON() false
#endif

// ---------------------------------------------------------------------------
// 읽기 모드 · 보기 설정 (글꼴, 크기, 색, 줄 간격, 여백)
static bool g_reading = false;                 // 읽기 모드: 결과 창만 크게
static HWND g_btnToc, g_btnFind, g_btnMark, g_btnFull;          // 읽기 칸 머리: 목차·찾기·책갈피·전체 화면
static bool g_fullscreen = false;
static HWND g_btnRead, g_btnTidy, g_btnVReset, g_cbVFont, g_cbVSize, g_cbVColor, g_btnVFg, g_btnVBg, g_cbVLine, g_cbVMargin, g_btnVBold;
static wstring g_vFont = L"맑은 고딕";
static int g_vSize = 11, g_vColor = 0, g_vLine = 1, g_vMargin = 1;  // g_vLine: 0 좁게 1 보통 2 조금 넓게 3 넓게 4 아주 넓게
static bool g_vBold = false;                   // 읽기 모드 글자 굵게
static COLORREF g_vfg = RGB(30, 30, 30), g_vbg = RGB(255, 255, 255);  // 사용자 지정 색
static COLORREF g_custCols[16];
static std::vector<wstring> g_fontNames;
static const int kSizes[] = {9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28};
static const int kLineSp[] = {0, 30, 60, 100};  // 줄 간격 보통~아주 넓게: 줄 뒤에 더하는 여백 (글자 크기의 %)
static const int kNarrowPct = 90;              // 줄 간격 좁게: 한 줄 높이의 90%
static bool g_newDoc = true;                   // 새 글이면 결과 창을 처음부터 (아니면 보던 곳 유지)
static long g_wantIdx = -1, g_wantLen = 0;     // 다시 연 파일의 지난번 읽던 곳
static long g_pendingTop = -1;                 // 큰 글: 정리 모드(앞부분만)에서 못 간 읽던 곳 → 읽기 모드에서 감
static wstring g_posPath;                      // 결과 창에 보이는 글의 파일
static wstring g_restoreNote;                  // "지난번 읽던 곳으로 이동" 알림
static int g_readPct = -1;

static void SaveReadPos();
static void LoadWantPos(const wstring& path);
static int ReadPercent();
// 색 조합: 0 기본 1 아이보리 2 세피아 3 보호 녹색 4 종이 회색 5 연하늘 6 저녁(앰버) 7 어두운 화면 8 사용자 지정
static const int kColorCustom = 8;
static void ViewColorsOf(int idx, COLORREF& fg, COLORREF& bg) {
    switch (idx) {
    case 1: fg = RGB(51, 51, 51);    bg = RGB(250, 246, 236); break;  // 아이보리
    case 2: fg = RGB(91, 70, 54);    bg = RGB(244, 236, 216); break;  // 세피아
    case 3: fg = RGB(34, 51, 34);    bg = RGB(199, 237, 204); break;  // 보호 녹색
    case 4: fg = RGB(51, 51, 51);    bg = RGB(236, 236, 236); break;  // 종이 회색
    case 5: fg = RGB(34, 48, 68);    bg = RGB(232, 240, 248); break;  // 연하늘
    case 6: fg = RGB(232, 196, 138); bg = RGB(36, 30, 22);    break;  // 저녁 (앰버)
    case 7: fg = RGB(204, 204, 204); bg = RGB(40, 42, 46);    break;  // 어두운 화면
    case 8: fg = g_vfg; bg = g_vbg; break;                           // 사용자 지정
    default: fg = col::editText; bg = col::editBg; break;  // 기본: 화면 테마 색
    }
}
static void ViewColors(COLORREF& fg, COLORREF& bg) { ViewColorsOf(g_vColor, fg, bg); }
static HFONT MakeReadFont() {
    return CreateFontW(-MulDiv(g_vSize * 10, g_dpi, 720), 0, 0, 0, g_vBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                       g_vFont.c_str());
}
static std::wstring RawRichText(HWND h, bool crlf);
static HFONT EditFontOf(HWND h);
// 글꼴에 들어 있는 글자 표 (BMP 65536자, 1비트씩). 알 수 없으면 모두 있다고 봄 (예전과 같이 동작)
static void BuildCoverage(HFONT f, std::vector<unsigned char>& cov) {
    cov.assign(8192, 0);
    bool ok = false;
    HDC dc = GetDC(nullptr);
    HGDIOBJ o = SelectObject(dc, f);
    DWORD sz = GetFontUnicodeRanges(dc, nullptr);
    if (sz) {
        std::vector<BYTE> buf(sz);
        GLYPHSET* gs = (GLYPHSET*)buf.data();
        gs->cbThis = sz;
        if (GetFontUnicodeRanges(dc, gs)) {
            ok = true;
            for (DWORD i = 0; i < gs->cRanges; ++i)
                for (unsigned c = gs->ranges[i].wcLow, e = c + gs->ranges[i].cGlyphs; c < e && c < 65536; ++c) cov[c >> 3] |= (unsigned char)(1 << (c & 7));
        }
    }
    SelectObject(dc, o);
    ReleaseDC(nullptr, dc);
    if (!ok) std::fill(cov.begin(), cov.end(), 0xFF);
}
// 두 글꼴(정리 칸·읽기 칸)까지 기억: 번갈아 물어도 다시 만들지 않음.
// 글꼴 핸들은 지운 뒤 다른 글꼴에 다시 쓰일 수 있으므로 글꼴 이름·굵기로 구별함
static const std::vector<unsigned char>& CoverageOf(HFONT f) {
    static wstring key[2];
    static std::vector<unsigned char> cov[2];
    static int next = 0;
    LOGFONTW lf = {};
    wstring k0;
    if (f && GetObjectW(f, sizeof(lf), &lf)) k0 = wstring(lf.lfFaceName) + L"|" + std::to_wstring(lf.lfWeight) + L"|" + std::to_wstring(lf.lfCharSet);
    if (!k0.empty())
        for (int i = 0; i < 2; ++i) if (key[i] == k0 && !cov[i].empty()) return cov[i];
    int k = next; next ^= 1;
    BuildCoverage(f, cov[k]);
    key[k] = k0;  // 알 수 없는 글꼴(빈 이름)은 다음에 다시 만듦
    return cov[k];
}
static bool HasGlyph(const std::vector<unsigned char>& cov, unsigned c) { return c < 65536 && (cov[c >> 3] & (1 << (c & 7))) != 0; }
// 따로 그릴 필요가 없는 글자: 제어 문자·공백·보이지 않는 서식 문자 (제로폭 등은 원래 글꼴에 그대로 둠)
static bool SkipForGlyph(unsigned c) {
    return c < 0x20 || c == 0x7F || c == 0x20 || c == 0xA0 || c == 0x3000 || c == 0x00AD || c == 0x034F || c == 0x061C || c == 0x180E ||
           (c >= 0x200B && c <= 0x200F) || (c >= 0x2028 && c <= 0x202E) || (c >= 0x2060 && c <= 0x206F) || (c >= 0xFE00 && c <= 0xFE0F) ||
           c == 0xFEFF || (c >= 0xFFF9 && c <= 0xFFFB);
}
// 글에 이 글꼴에 없는 글자(예: 맑은 고딕에 없는 중국 간체자 别·让·妈)가 있는지
static bool FontLacksGlyphs(HFONT f, const std::wstring& s) {
    if (!f) return false;
    const auto& cov = CoverageOf(f);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned c = s[i];
        if (SkipForGlyph(c)) continue;
        if (c >= 0xD800 && c <= 0xDBFF) return true;  // BMP 밖 글자(확장 한자·그림 문자 등)
        if (c >= 0xDC00 && c <= 0xDFFF) continue;
        if (!HasGlyph(cov, c)) return true;
    }
    return false;
}
// 없는 글자를 대신 그릴 글꼴 후보 (Windows 기본 글꼴). 설치되어 있고 그 글자가 있는 첫 글꼴을 씀
struct FbFont { const wchar_t* face; BYTE charset; };
static const FbFont kFallbackFonts[] = {
    {L"Microsoft YaHei", GB2312_CHARSET}, {L"SimSun", GB2312_CHARSET}, {L"Microsoft JhengHei", CHINESEBIG5_CHARSET}, {L"MingLiU", CHINESEBIG5_CHARSET},
    {L"Yu Gothic", SHIFTJIS_CHARSET}, {L"Meiryo", SHIFTJIS_CHARSET}, {L"MS Gothic", SHIFTJIS_CHARSET}, {L"Segoe UI Symbol", DEFAULT_CHARSET},
    {L"Segoe UI", DEFAULT_CHARSET}, {L"Nirmala UI", DEFAULT_CHARSET}, {L"Leelawadee UI", DEFAULT_CHARSET}, {L"Ebrima", DEFAULT_CHARSET},
    {L"Gadugi", DEFAULT_CHARSET}, {L"Segoe UI Historic", DEFAULT_CHARSET}, {L"Arial Unicode MS", DEFAULT_CHARSET},
};
static const int kFbCount = (int)(sizeof(kFallbackFonts) / sizeof(kFallbackFonts[0]));
static const std::vector<unsigned char>* FallbackCoverage(int i) {  // 설치 안 됐으면 nullptr
    static std::vector<std::vector<unsigned char>> covs(kFbCount);
    static std::vector<int> state(kFbCount, 0);  // 0 모름, 1 있음, 2 없음
    if (state[i] == 0) {
        HFONT f = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, kFallbackFonts[i].face);
        state[i] = 2;
        if (f) {
            HDC dc = GetDC(nullptr);
            HGDIOBJ o = SelectObject(dc, f);
            wchar_t got[LF_FACESIZE] = {};
            GetTextFaceW(dc, LF_FACESIZE, got);
            SelectObject(dc, o);
            ReleaseDC(nullptr, dc);
            if (lstrcmpiW(got, kFallbackFonts[i].face) == 0) { BuildCoverage(f, covs[i]); state[i] = 1; }
            DeleteObject(f);
        }
    }
    return state[i] == 1 ? &covs[i] : nullptr;
}
static int InstalledFace(const wchar_t* face) {  // BMP 밖 글자용: 설치 여부만
    for (int i = 0; i < kFbCount; ++i) if (lstrcmpiW(kFallbackFonts[i].face, face) == 0) return FallbackCoverage(i) ? i : -1;
    return -1;
}
// 글꼴에 없는 글자에만 그 글자가 있는 글꼴을 직접 지정 (글자는 그대로, 화면 표시만).
// 제로폭 같은 보이지 않는 문자와 한글은 원래 글꼴에 그대로 남겨, Windows 자동 글꼴 바꿈이 앞뒤 글자를 보고 잘못 고르는 일을 막음.
// 글 칸이 부분 글꼴 지정을 받지 않으면(다른 글자 글꼴까지 바뀌면) 되돌리고 false
static bool ApplyGlyphFallback(HWND h) {
    HFONT bf = EditFontOf(h);
    if (!bf) return false;
    LOGFONTW lf = {};
    GetObjectW(bf, sizeof(lf), &lf);
    const auto& base = CoverageOf(bf);
    std::wstring t = RawRichText(h, false);  // 글 칸 안의 위치와 1:1 (줄바꿈은 CR 한 글자)
    struct Run { LONG a, b; int font; };
    std::vector<Run> runs;
    long probe = -1;  // 원래 글꼴로 남아야 하는 글자 하나 (확인용)
    for (size_t i = 0; i < t.size(); ++i) {
        unsigned c = t[i];
        int font = -1;
        size_t len = 1;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < t.size()) {  // BMP 밖: 확장 한자 / 그림 문자
            len = 2;
            if (c >= 0xD840 && c <= 0xD87F) font = InstalledFace(L"SimSun");
            if (c >= 0xD83C && c <= 0xD83E) font = InstalledFace(L"Segoe UI Symbol");
        } else if (SkipForGlyph(c) || HasGlyph(base, c)) {
            if (probe < 0 && !SkipForGlyph(c)) probe = (long)i;
            continue;
        } else {
            for (int k = 0; k < kFbCount; ++k) {
                const auto* cv = FallbackCoverage(k);
                if (cv && HasGlyph(*cv, c)) { font = k; break; }
            }
        }
        if (font >= 0) {
            if (!runs.empty() && runs.back().b == (LONG)i && runs.back().font == font) runs.back().b = (LONG)(i + len);
            else runs.push_back({(LONG)i, (LONG)(i + len), font});
        }
        i += len - 1;
    }
    if (runs.empty()) return true;
    // 열리는 파일의 크기·PC 속도를 미리 알 수 없으므로 개수가 아니라 실제 걸리는 시간으로 정함:
    // 처음 64개를 지정한 속도로 전체 시간을 어림해 1.5초를 넘을 것 같으면, 또는 실제로 2초를 넘으면 되돌리고 자동 글꼴 바꿈에 맡김
    // (첫 묶음은 대신 그릴 글꼴을 처음 불러오는 시간이 섞이므로 그다음부터 잼)
    LARGE_INTEGER qf, q0;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&q0);
    auto elapsedMs = [&]() { LARGE_INTEGER q; QueryPerformanceCounter(&q); return (double)(q.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart; };
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_CHARSET;
    for (size_t r = 0; r < runs.size(); ++r) {
        CHARRANGE cr = {runs[r].a, runs[r].b};
        SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);
        lstrcpynW(cf.szFaceName, kFallbackFonts[runs[r].font].face, LF_FACESIZE);
        cf.bCharSet = kFallbackFonts[runs[r].font].charset;
        SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        // (큰 글은 뒤쪽 묶음 하나가 몇 초씩 걸릴 수 있어 2초 넘음은 묶음마다 확인)
        if ((r == 64 && elapsedMs() / 64.0 * (double)(runs.size() - 1) > 1500.0) || (r > 0 && elapsedMs() > 2000.0)) {
            lstrcpynW(cf.szFaceName, lf.lfFaceName, LF_FACESIZE);  // 너무 오래 걸림: 되돌림
            cf.bCharSet = lf.lfCharSet;
            SendMessageW(h, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
            return false;
        }
        if (r == 0) {  // 첫 지정 뒤 확인: 대상 글자는 바뀌고, 다른 글자는 그대로여야 함 (아니면 이 글 칸은 부분 지정을 못 함)
            auto faceAt = [&](LONG pos) {
                CHARRANGE pr = {pos, pos + 1};
                SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&pr);
                CHARFORMAT2W chk = {};
                chk.cbSize = sizeof(chk);
                SendMessageW(h, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&chk);
                return std::wstring(chk.szFaceName);
            };
            bool runOk = lstrcmpiW(faceAt(runs[0].a).c_str(), kFallbackFonts[runs[0].font].face) == 0;
            bool restOk = probe < 0 || lstrcmpiW(faceAt(probe).c_str(), kFallbackFonts[runs[0].font].face) != 0;
            if (!runOk || !restOk) {
                lstrcpynW(cf.szFaceName, lf.lfFaceName, LF_FACESIZE);  // 되돌림
                cf.bCharSet = lf.lfCharSet;
                SendMessageW(h, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
                return false;
            }
            QueryPerformanceCounter(&q0);  // 시간은 여기서부터 잼
        }
    }
    return true;
}
static bool IsRichMode(HWND h) { return (SendMessageW(h, EM_GETTEXTMODE, 0, 0) & TM_RICHTEXT) != 0; }
// 서식 텍스트 방식의 글 칸에서 글을 치거나 붙여 넣을 때: 커서 앞이 대신 그린 글자여도 원래 글꼴로 들어가게
static void UseBaseFontAtCaret(HWND h) {
    HFONT bf = EditFontOf(h);
    if (!bf || !IsRichMode(h) || (GetWindowLongW(h, GWL_STYLE) & ES_READONLY)) return;  // 고칠 수 없는 칸(읽기 모드)은 손대지 않음
    LOGFONTW lf = {};
    GetObjectW(bf, sizeof(lf), &lf);
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_CHARSET;
    lstrcpynW(cf.szFaceName, lf.lfFaceName, LF_FACESIZE);
    cf.bCharSet = lf.lfCharSet;
    SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}
static void SetAutoFont(HWND h, bool on) {
    LRESULT lo = SendMessageW(h, EM_GETLANGOPTIONS, 0, 0);
    LRESULT want = (lo & ~(IMF_AUTOFONT | IMF_AUTOFONTSIZEADJUST | IMF_DUALFONT)) | (on ? IMF_AUTOFONT : 0);
    if (want != lo) SendMessageW(h, EM_SETLANGOPTIONS, 0, want);
}
static long RichLen(HWND h) {
    GETTEXTLENGTHEX gl = {GTL_NUMCHARS | GTL_PRECISE, 1200};
    return (long)SendMessageW(h, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
}
static long TopChar(HWND h) {
    long line = (long)SendMessageW(h, EM_GETFIRSTVISIBLELINE, 0, 0);
    long c = (long)SendMessageW(h, EM_LINEINDEX, line, 0);
    return c < 0 ? 0 : c;
}
// 글자 c가 있는 줄이 맨 위에 오도록
// RichEdit의 스크롤 위치 값은 16비트(65535px)까지라, 글이 그보다 길면 줄 단위로 옮김
static void ScrollToChar(HWND h, long c) {
    long len = RichLen(h);
    if (c > len) c = len;
    if (c < 0) c = 0;
    long line = (long)SendMessageW(h, EM_EXLINEFROMCHAR, 0, c);
    long ls = (long)SendMessageW(h, EM_LINEINDEX, line, 0);
    if (ls < 0) ls = 0;
    SCROLLINFO si = {sizeof(si), SIF_RANGE};
    if (GetScrollInfo(h, SB_VERT, &si) && si.nMax < 60000) {  // 짧은 글: 픽셀 단위로 정확히
        RECT fr; SendMessageW(h, EM_GETRECT, 0, (LPARAM)&fr);
        POINTL pt = {}; SendMessageW(h, EM_POSFROMCHAR, (WPARAM)&pt, ls);
        POINT cur = {}; SendMessageW(h, EM_GETSCROLLPOS, 0, (LPARAM)&cur);
        POINT np = {0, cur.y + (pt.y - fr.top)};
        if (np.y < 0) np.y = 0;
        if (np.y != cur.y) SendMessageW(h, EM_SETSCROLLPOS, 0, (LPARAM)&np);
        return;
    }
    for (int it = 0; it < 40; ++it) {  // 긴 글: 줄 단위
        long first = (long)SendMessageW(h, EM_GETFIRSTVISIBLELINE, 0, 0);
        if (first == line) break;
        SendMessageW(h, EM_LINESCROLL, 0, line - first);
        if ((long)SendMessageW(h, EM_GETFIRSTVISIBLELINE, 0, 0) == first) break;  // 더 갈 수 없음
    }
}
// 정리 모드의 줄 높이(twips): 글꼴 본래 높이로 고정해 원본 칸과 결과 칸이 똑같은 줄 간격이 되게 함
// (줄 안에 기본 글꼴에 없는 글자·보이지 않는 서식 문자가 있으면 Windows가 다른 글꼴을 빌려 그 줄만 높아지는 것을 막음)
static LONG EditLineTwips(HWND h, HFONT f) {
    if (!f) return 0;
    HDC dc = GetDC(h);
    HGDIOBJ old = SelectObject(dc, f);
    TEXTMETRICW tm = {};
    BOOL ok = GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    if (!ok || tm.tmHeight <= 0) return 0;
    return MulDiv(tm.tmHeight, 1440, g_dpi);
}
// 글자색·바탕색·줄 간격 (보던 위치와 선택은 그대로)
// 정리 모드는 늘 기본 모양(흰 바탕), 보기 설정은 읽기 모드의 결과 창에만
static void ApplyEditLook(HWND h) {
    if (!h) return;
    COLORREF fg, bg;
    bool view = g_reading && h == g_dst;
    if (view) ViewColors(fg, bg);
    else { fg = col::editText; bg = col::editBg; }
    bool ol = g_loading, os = g_syncing;
    g_loading = true; g_syncing = true;  // 바뀜·선택 알림 무시
    long top = TopChar(h);
    CHARRANGE cr; SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&cr);
    SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    SendMessageW(h, EM_SETBKGNDCOLOR, 0, bg);
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = fg;
    SendMessageW(h, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SendMessageW(h, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    PARAFORMAT2 pf = {};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_SPACEAFTER | PFM_LINESPACING;  // 줄(문단) 뒤 여백: 글자 크기에 비례
    int vl = (g_vLine >= 0 && g_vLine <= 4) ? g_vLine : 1;
    int lsp = (view && vl >= 1) ? kLineSp[vl - 1] : 0;  // 줄 간격은 읽기 모드에서만
    bool narrow = view && vl == 0;                     // 좁게: 줄 높이를 글꼴 높이의 90%로 고정
    pf.dySpaceAfter = g_vSize * 20 * lsp / 100;  // twips
    // 줄 높이를 글꼴 높이로 고정 (원본·결과 칸이 같은 간격). 읽기 모드도 같은 높이가 기준:
    // 보통 = 정리 모드와 같음, 좁게 = 그 90%, 조금 넓게부터 = 보통 + 줄 뒤 여백
    // (고정하지 않으면 Windows 기본 한 줄 간격이 정리 모드보다 넓고, 다른 글꼴 글자가 낀 줄만 더 높아짐)
    LONG baseLine = EditLineTwips(h, EditFontOf(h));
    LONG fixedLine = (view && narrow) ? baseLine * kNarrowPct / 100 : baseLine;
    pf.bLineSpacingRule = fixedLine > 0 ? 4 : 0;  // 4: 정확히 dyLineSpacing
    pf.dyLineSpacing = fixedLine;
    static LONG lastSp[2] = {0, 0}, lastLs[2] = {-1, -1};  // 창마다 마지막으로 준 값 (바뀔 때만 전체에 적용: 큰 글에서 빠르게)
    LONG& last = lastSp[h == g_dst ? 1 : 0];
    LONG& lastL = lastLs[h == g_dst ? 1 : 0];
    if (pf.dySpaceAfter != last || fixedLine != lastL || RichLen(h) == 0) {  // 바뀌었을 때만 (빈 글에서는 늘 정해 둠)
        SendMessageW(h, EM_SETSEL, 0, -1);
        SendMessageW(h, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
        SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);
        last = pf.dySpaceAfter;
        lastL = fixedLine;
    }
    ScrollToChar(h, top);
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(h, nullptr, TRUE);
    g_loading = ol; g_syncing = os;
}

static const wchar_t kFilter[] =  // 저장
    L"텍스트 파일 (*.txt;*.cap;*.text;*.log;*.asc;*.nfo;*.md;*.ini)\0*.txt;*.cap;*.text;*.log;*.asc;*.nfo;*.md;*.ini\0"
    L"모든 파일 (*.*)\0*.*\0";
static const wchar_t kOpenFilter[] =  // 열기
    L"텍스트·EPUB 파일 (*.txt;*.cap;*.text;*.log;*.asc;*.nfo;*.md;*.ini;*.epub)\0*.txt;*.cap;*.text;*.log;*.asc;*.nfo;*.md;*.ini;*.epub\0"
    L"텍스트 파일 (*.txt;*.cap;*.text;*.log;*.asc;*.nfo;*.md;*.ini)\0*.txt;*.cap;*.text;*.log;*.asc;*.nfo;*.md;*.ini\0"
    L"EPUB 전자책 (*.epub)\0*.epub\0"
    L"모든 파일 (*.*)\0*.*\0";

// ---------------------------------------------------------------------------
// 문자 변환
static U32 Utf8ToU32(const unsigned char* p, size_t n) {
    U32 r; r.reserve(n);
    size_t i = 0;
    while (i < n) {
        unsigned c = p[i];
        int len; char32_t cp;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { r += 0xFFFD; ++i; continue; }
        if (i + len > n) { r += 0xFFFD; break; }
        bool bad = false;
        for (int k = 1; k < len; ++k) { if ((p[i + k] & 0xC0) != 0x80) { bad = true; break; } cp = (cp << 6) | (p[i + k] & 0x3F); }
        if (bad) { r += 0xFFFD; ++i; continue; }
        r += cp; i += len;
    }
    return r;
}
// UTF-8로 올바른 멀티바이트 글자 수와 잘못된 바이트 수
static void Utf8Stats(const unsigned char* p, size_t n, size_t& goodMulti, size_t& bad) {
    goodMulti = bad = 0;
    size_t i = 0;
    while (i < n) {
        unsigned c = p[i];
        int len;
        if (c < 0x80) { ++i; continue; }
        else if (c >= 0xC2 && c <= 0xDF) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if (c >= 0xF0 && c <= 0xF4) len = 4;
        else { bad++; ++i; continue; }
        if (i + len > n) { if (n - i >= 4 || i == 0) bad++; break; }  // 파일 끝에서 잘린 글자는 봐줌
        bool ok = true;
        for (int k = 1; k < len; ++k) if ((p[i + k] & 0xC0) != 0x80) { ok = false; break; }
        if (ok && len == 3) {
            unsigned c1 = p[i + 1];
            if ((c == 0xE0 && c1 < 0xA0) || (c == 0xED && c1 >= 0xA0)) ok = false;
        }
        if (!ok) { bad++; ++i; continue; }
        goodMulti++;
        i += len;
    }
}
// BOM 없는 UTF-16 판별: 0 아님, 1 LE, 2 BE
// (1) 영문·숫자가 섞인 글: 한쪽 바이트에만 0이 많이 나옴
// (2) 한글만 있는 글: 윗바이트가 한글 영역(AC~D7)이고 아랫바이트는 0x80 미만도 고르게 나옴
//     (CP949 한글은 두 바이트 모두 0x81 이상이라 섞일 일이 없음)
static int SniffUtf16(const unsigned char* u, size_t n) {
    if (n < 4) return 0;
    size_t pairs = n / 2 < 8192 ? n / 2 : 8192, zo = 0, ze = 0;
    for (size_t i = 0; i < pairs; ++i) { if (u[2 * i + 1] == 0) zo++; if (u[2 * i] == 0) ze++; }
    if (zo > pairs * 3 / 10 && ze < pairs / 20) return 1;
    if (ze > pairs * 3 / 10 && zo < pairs / 20) return 2;
    if (n % 2 != 0 || pairs < 4) return 0;
    for (int be = 0; be < 2; ++be) {
        size_t hang = 0, lowSmall = 0, other = 0;
        for (size_t i = 0; i < pairs; ++i) {
            unsigned hi = be ? u[2 * i] : u[2 * i + 1], lo = be ? u[2 * i + 1] : u[2 * i];
            if (hi >= 0xAC && hi <= 0xD7) { hang++; if (lo < 0x80) lowSmall++; }
            else if (hi == 0x00 || hi == 0x20 || hi == 0x30 || hi == 0x31 || hi == 0xFF) {}
            else other++;
        }
        if (hang >= pairs / 2 && other <= pairs / 20 && lowSmall * 5 >= hang) return be ? 2 : 1;
    }
    return 0;
}

static U32 Utf16ToU32(const unsigned char* p, size_t n, bool be) {
    U32 r; r.reserve(n / 2);
    size_t cnt = n / 2;
    auto at = [&](size_t i) -> unsigned { return be ? (p[2 * i] << 8) | p[2 * i + 1] : p[2 * i] | (p[2 * i + 1] << 8); };
    for (size_t i = 0; i < cnt; ++i) {
        unsigned u = at(i);
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < cnt) {
            unsigned v = at(i + 1);
            if (v >= 0xDC00 && v <= 0xDFFF) { r += (char32_t)(0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00)); ++i; continue; }
        }
        r += (char32_t)u;
    }
    return r;
}
static U32 WToU32(const wchar_t* p, size_t n) {
    U32 r; r.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        unsigned u = (unsigned short)p[i];
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) {
            unsigned v = (unsigned short)p[i + 1];
            if (v >= 0xDC00 && v <= 0xDFFF) { r += (char32_t)(0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00)); ++i; continue; }
        }
        r += (char32_t)u;
    }
    return r;
}
static wstring U32ToW(const U32& s, bool crlf) {
    wstring r; r.reserve(s.size() + s.size() / 16);
    for (char32_t c : s) {
        if (c == '\n' && crlf) { r += L"\r\n"; continue; }
        if (c >= 0x10000) { c -= 0x10000; r += (wchar_t)(0xD800 + (c >> 10)); r += (wchar_t)(0xDC00 + (c & 0x3FF)); }
        else r += (wchar_t)c;
    }
    return r;
}

// 줄바꿈을 \n 하나로 통일 (CRLF를 그대로 두면 표시할 때 \r\r\n이 되어 줄이 붙어 보임)
static U32 NormalizeNL(const U32& s) {
    U32 r; r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char32_t c = s[i];
        if (c == '\r') { r += U'\n'; if (i + 1 < s.size() && s[i + 1] == '\n') ++i; }
        else r += c;
    }
    return r;
}

static U32 DecodeAuto(const std::string& b, int& enc);
// force: 0 자동, 1 UTF-8, 2 CP949, 3 UTF-16, 4 Shift-JIS, 5 GBK, 6 Big5, 7 Windows-1252, 8 Johab
static U32 Decode(const std::string& b, int& enc, int force = -1);
static int g_forceEnc = 0;
static U32 Decode(const std::string& b, int& enc, int force) {
    if (force < 0) force = g_forceEnc;
    const unsigned char* u = (const unsigned char*)b.data();
    size_t n = b.size();
    if (force == 1) {
        bool bom = n >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF;
        enc = bom ? E_UTF8BOM : E_UTF8;
        return bom ? Utf8ToU32(u + 3, n - 3) : Utf8ToU32(u, n);
    }
    if (force == 2 || (force >= 4 && force <= 8)) {
        static const int fe[] = {0, 0, E_CP949, 0, E_SJIS, E_GBK, E_BIG5, E_W1252, E_JOHAB};
        enc = fe[force];
        UINT cp = EncCp(enc);
        int len = MultiByteToWideChar(cp, 0, b.data(), (int)n, nullptr, 0);
        wstring w(len > 0 ? len : 0, 0);
        if (len > 0) MultiByteToWideChar(cp, 0, b.data(), (int)n, &w[0], len);
        return WToU32(w.data(), w.size());
    }
    if (force == 3) {
        bool be = n >= 2 && u[0] == 0xFE && u[1] == 0xFF;
        bool bom = n >= 2 && ((u[0] == 0xFF && u[1] == 0xFE) || be);
        enc = be ? E_UTF16BE : E_UTF16LE;
        return bom ? Utf16ToU32(u + 2, n - 2, be) : Utf16ToU32(u, n, false);
    }
    return DecodeAuto(b, enc);
}
static U32 DecodeAuto(const std::string& b, int& enc) {
    const unsigned char* u = (const unsigned char*)b.data();
    size_t n = b.size();
    if (n >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) { enc = E_UTF8BOM; return Utf8ToU32(u + 3, n - 3); }
    if (n >= 2 && u[0] == 0xFF && u[1] == 0xFE) { enc = E_UTF16LE; return Utf16ToU32(u + 2, n - 2, false); }
    if (n >= 2 && u[0] == 0xFE && u[1] == 0xFF) { enc = E_UTF16BE; return Utf16ToU32(u + 2, n - 2, true); }
    int u16 = SniffUtf16(u, n);  // BOM 없는 UTF-16
    if (u16 == 1) { enc = E_UTF16LE; return Utf16ToU32(u, n, false); }
    if (u16 == 2) { enc = E_UTF16BE; return Utf16ToU32(u, n, true); }
    {
        // UTF-8: 완전히 올바르거나, 일부 바이트만 손상된 UTF-8이면 UTF-8로 읽음
        // (손상된 한두 곳 때문에 글 전체가 깨져 보이지 않게, 그 자리만 표시)
        size_t good, bad;
        Utf8Stats(u, n, good, bad);
        if (bad == 0 || (good >= 20 && bad * 200 <= good)) { enc = E_UTF8; return Utf8ToU32(u, n); }
    }
    // 유니코드가 아닌 글: 앞부분(최대 512KB, 줄 끝에서 자름)으로 어느 코드 페이지인지 판별
    enc = E_CP949;
    {
        // 0x30 미만 바이트(공백·줄바꿈·문장부호)는 어느 인코딩에서도 두 바이트 글자의 둘째 바이트가 아니므로 거기서 자름
        auto safeCut = [&](size_t from, size_t want) {
            size_t e = want;
            while (e > from && u[e - 1] >= 0x30) --e;
            return e > from ? e : want;
        };
        size_t sn = n > 512 * 1024 ? safeCut(0, 512 * 1024) : n;
        // 조각(줄 또는 4KB)마다 엄격히 읽어 보고, 98% 이상이 올바르면 그 인코딩일 수 있다고 봄
        // (손상된 바이트 한두 개 때문에 글 전체를 다른 인코딩으로 보지 않게) · 점수는 너그럽게 읽은 글로 셈
        auto strict = [&](UINT cp, wstring& w) -> bool {
            size_t chunks = 0, bad = 0, i = 0;
            while (i < sn) {
                size_t e = i;
                while (e < sn && u[e] != '\n' && e - i < 4096) ++e;
                if (e < sn && u[e] != '\n') e = safeCut(i, e);
                if (e < sn) ++e;  // 줄바꿈이나 자른 바이트 포함
                if (e <= i) e = std::min(sn, i + 4096);
                bool hi = false;
                for (size_t k = i; k < e; ++k) if (u[k] >= 0x80) { hi = true; break; }
                if (hi) {
                    chunks++;
                    bool okc = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, b.data() + i, (int)(e - i), nullptr, 0) > 0;
                    if (!okc && e < sn &&  // 두 바이트 글자 한가운데서 잘렸으면 한 바이트 늘려 다시
                        MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, b.data() + i, (int)(e + 1 - i), nullptr, 0) > 0) {
                        okc = true;
                        ++e;
                    }
                    if (!okc) bad++;
                }
                i = e;
            }
            if (chunks && bad * 50 > chunks) return false;
            int len = MultiByteToWideChar(cp, 0, b.data(), (int)sn, nullptr, 0);
            if (len <= 0) return false;
            w.assign(len, 0);
            return MultiByteToWideChar(cp, 0, b.data(), (int)sn, &w[0], len) > 0;
        };
        // 그 언어에서 가장 자주 쓰는 글자의 비율: 잘못 읽으면 엉뚱한 글자가 나와 비율이 크게 떨어짐
        auto cover = [](const wstring& w, const wchar_t* set) {
            size_t hit = 0, na = 0;
            for (wchar_t c : w) { if ((unsigned short)c < 0x80) continue; na++; if (wcschr(set, c)) hit++; }
            return na ? (double)hit / na : 0.0;
        };
        auto range = [](const wstring& w, unsigned lo, unsigned hi) {
            size_t hit = 0, na = 0;
            for (wchar_t c : w) { unsigned v = (unsigned short)c; if (v < 0x80) continue; na++; if (v >= lo && v <= hi) hit++; }
            return na ? (double)hit / na : 0.0;
        };
        static const wchar_t kKo[] = L"이다는의에고을를가한하지서로으기도사대수리자어아나게그니인일해있시내었것들정면만보라거요말았주우소오전";
        static const wchar_t kZhS[] = L"的一是不了在人有我他这个们中来上大为和国地到以说时要就出会可也你对生能而子那得于着下自之年过发后作里用道行所然家种事成方多经么去法学如都同现当没动面起看定天分还进好小部其些主样理心她本前开但因只从想实";
        static const wchar_t kZhT[] = L"的一是不了在人有我他這個們中來上大為和國地到以說時要就出會可也你對生能而子那得於著下自之年過發後作裡用道行所然家種事成方多經麼去法學如都同現當沒動面起看定天分還進好小部其些主樣理心她本前開但因只從想實";
        wstring w;
        bool ok949 = strict(949, w);
        if (!ok949) {  // 손상이 많아도 한국어 글이면 CP949로 (예전처럼)
            int len = MultiByteToWideChar(949, 0, b.data(), (int)sn, nullptr, 0);
            w.assign(len > 0 ? len : 0, 0);
            if (len > 0) MultiByteToWideChar(949, 0, b.data(), (int)sn, &w[0], len);
        }
        double ko949 = cover(w, kKo);
        double koJohab = strict(1361, w) ? cover(w, kKo) : 0;
        wstring w932;
        bool ok932 = strict(932, w932);
        double ja = ok932 ? range(w932, 0x3041, 0x309F) : 0;           // 히라가나 비율
        double zs = strict(936, w) ? cover(w, kZhS) : 0;
        double zt = strict(950, w) ? cover(w, kZhT) : 0;
        if (ko949 >= 0.12 && ko949 >= koJohab) enc = E_CP949;           // 한국어
        else if (koJohab >= 0.12) enc = E_JOHAB;                         // 옛 한글 조합형
        else if (ja >= 0.12) enc = E_SJIS;                               // 일본어
        else if (zs >= 0.08 || zt >= 0.08) enc = zs >= zt ? E_GBK : E_BIG5;  // 중국어
        else if (ok949) enc = E_CP949;
        else if (ok932 && range(w932, 0x4E00, 0x9FFF) >= 0.5) enc = E_SJIS;  // 가나 없이 한자만 있는 일본어
        else if (strict(1252, w)) enc = E_W1252;                         // 서유럽
        else enc = E_CP949;
    }
    UINT cp = EncCp(enc);
    int len = MultiByteToWideChar(cp, 0, b.data(), (int)n, nullptr, 0);
    if (len <= 0) { cp = CP_ACP; len = MultiByteToWideChar(cp, 0, b.data(), (int)n, nullptr, 0); }
    wstring w(len, 0);
    if (len > 0) MultiByteToWideChar(cp, 0, b.data(), (int)n, &w[0], len);
    return WToU32(w.data(), w.size());
}

static std::string Encode(const wstring& w, int enc, int* lossyCount) {
    std::string out;
    if (lossyCount) *lossyCount = 0;
    if (enc == E_UTF16LE || enc == E_UTF16BE) {
        out.reserve(w.size() * 2 + 2);
        if (enc == E_UTF16LE) { out += '\xFF'; out += '\xFE'; } else { out += '\xFE'; out += '\xFF'; }
        for (wchar_t c : w) {
            unsigned u = (unsigned short)c;
            if (enc == E_UTF16LE) { out += (char)(u & 0xFF); out += (char)(u >> 8); }
            else { out += (char)(u >> 8); out += (char)(u & 0xFF); }
        }
        return out;
    }
    if (UINT cp = EncCp(enc)) {
        // 비슷한 글자로 몰래 바꾸지 않음 (ā→a 등): 표현할 수 없으면 '?'로 바꾸고 알림
        int len = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        out.resize(len);
        BOOL used = FALSE;
        if (len > 0) WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, w.data(), (int)w.size(), &out[0], len, "?", &used);
        if (used && lossyCount) {
            // 되돌려 읽어 원문과 다른 글자 수를 셈 (한 글자씩 바꿔 보는 것보다 훨씬 빠름)
            int cnt = 0;
            int bl = MultiByteToWideChar(cp, 0, out.data(), (int)out.size(), nullptr, 0);
            wstring back(bl > 0 ? bl : 0, 0);
            if (bl > 0) MultiByteToWideChar(cp, 0, out.data(), (int)out.size(), &back[0], bl);
            if (back.size() == w.size()) {
                for (size_t i = 0; i < w.size(); ++i) if (back[i] != w[i]) cnt++;
            } else {
                for (wchar_t c : w) {
                    if (c < 0x80) continue;
                    char tmp[8]; BOOL u2 = FALSE;
                    WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, &c, 1, tmp, 8, "?", &u2);
                    if (u2) cnt++;
                }
            }
            *lossyCount = cnt ? cnt : 1;
        }
        return out;
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (enc == E_UTF8BOM) out = "\xEF\xBB\xBF";
    size_t off = out.size();
    out.resize(off + len);
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[off], len, nullptr, nullptr);
    return out;
}

static bool ReadAll(const wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > 512LL * 1024 * 1024) { CloseHandle(h); return false; }
    out.resize((size_t)sz.QuadPart);
    size_t got = 0;
    while (got < out.size()) {
        DWORD rd = 0;
        DWORD want = (DWORD)((out.size() - got) > (1u << 30) ? (1u << 30) : (out.size() - got));
        if (!ReadFile(h, &out[got], want, &rd, nullptr) || rd == 0) break;
        got += rd;
    }
    CloseHandle(h);
    out.resize(got);
    return true;
}
static bool WriteAll(const wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    size_t done = 0; bool ok = true;
    while (done < data.size()) {
        DWORD wr = 0;
        DWORD want = (DWORD)((data.size() - done) > (1u << 30) ? (1u << 30) : (data.size() - done));
        if (!WriteFile(h, data.data() + done, want, &wr, nullptr) || wr == 0) { ok = false; break; }
        done += wr;
    }
    CloseHandle(h);
    return ok;
}
// ---------------------------------------------------------------------------
// 열 수 있는 파일인지 미리 검사 (파일 전체를 읽기 전에 앞부분만 보고 판단)
static const long long kMaxOpenBytes = 100LL * 1024 * 1024;   // 여는 파일 최대 크기 (큰 파일은 앞부분만 화면에 보임)
static const long long kMaxBatchBytes = 100LL * 1024 * 1024;  // 여러 파일 변환·합치기 최대 크기
static const long long kMaxEpubBytes = 300LL * 1024 * 1024;  // EPUB 파일 최대 크기 (그림이 많아도 본문 글은 작음)
static const size_t kPreviewChars = 600000;   // 이보다 긴 글은 화면에 앞부분만 표시 (정리·저장은 전체)
static const size_t kAsyncChars = 300000;     // 이보다 긴 글은 정리를 뒤에서 돌려 창이 멈추지 않게

static wstring ExtOf(const wstring& p) {
    size_t s = p.find_last_of(L"\\/"), d = p.find_last_of(L'.');
    if (d == wstring::npos || (s != wstring::npos && d < s)) return L"";
    wstring e = p.substr(d + 1);
    for (auto& c : e) c = (wchar_t)towlower(c);
    return e;
}

// 반환: 빈 문자열이면 열 수 있음, 아니면 열 수 없는 이유
static wstring CheckTextFile(const wstring& path, long long maxBytes) {
    // 1) 확장자로 바로 알 수 있는 형식
    static const wchar_t* archive[] = {L"zip", L"rar", L"7z", L"alz", L"egg", L"gz", L"tgz", L"bz2", L"xz", L"tar", L"cab", L"iso", L"lzh", nullptr};
    static const wchar_t* exe[] = {L"exe", L"dll", L"msi", L"sys", L"com", L"scr", L"bin", L"dat", nullptr};
    static const wchar_t* doc[] = {L"pdf", L"doc", L"docx", L"hwp", L"hwpx", L"xls", L"xlsx", L"ppt", L"pptx", L"odt", L"rtf", nullptr};
    static const wchar_t* img[] = {L"jpg", L"jpeg", L"png", L"gif", L"bmp", L"webp", L"tif", L"tiff", L"ico", L"psd", L"heic", nullptr};
    static const wchar_t* media[] = {L"mp3", L"wav", L"flac", L"ogg", L"m4a", L"mp4", L"avi", L"mkv", L"mov", L"wmv", L"webm", nullptr};
    wstring e = ExtOf(path);
    if (e == L"epub") {  // EPUB 전자책: ZIP 파일인지만 보고, 본문은 열 때 꺼냄
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return L"파일을 열 수 없음";
        LARGE_INTEGER sz;
        unsigned char sig[4] = {};
        DWORD rd = 0;
        bool ok = GetFileSizeEx(h, &sz) && ReadFile(h, sig, 4, &rd, nullptr) && rd == 4;
        CloseHandle(h);
        if (!ok || memcmp(sig, "PK\x03\x04", 4) != 0) return L"손상되었거나 EPUB이 아닌 파일";
        if (sz.QuadPart > kMaxEpubBytes) return L"EPUB 파일이 너무 큼 (최대 " + std::to_wstring(kMaxEpubBytes / (1024 * 1024)) + L"MB)";
        return L"";
    }
    auto in = [&](const wchar_t** l) { for (; *l; ++l) if (e == *l) return true; return false; };
    if (in(archive)) return L"압축 파일";
    if (in(exe)) return L"실행 파일 또는 데이터 파일";
    if (in(doc)) return L"문서 파일 (PDF·워드·한글 등)";
    if (in(img)) return L"그림 파일";
    if (in(media)) return L"음악·동영상 파일";

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"파일을 열 수 없음";
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { CloseHandle(h); return L"파일을 열 수 없음"; }
    if (sz.QuadPart > maxBytes) {
        CloseHandle(h);
        return L"파일이 너무 큼 (최대 " + std::to_wstring(maxBytes / (1024 * 1024)) + L"MB)";
    }
    unsigned char buf[65536];
    DWORD rd = 0;
    ReadFile(h, buf, sizeof(buf), &rd, nullptr);
    CloseHandle(h);
    if (rd == 0) return L"";  // 빈 파일
    auto starts = [&](const char* m, size_t n, size_t off = 0) { return rd >= off + n && memcmp(buf + off, m, n) == 0; };

    // 2) 파일 앞머리 표식으로 알 수 있는 형식 (확장자를 바꿔 놓은 경우)
    if (starts("PK\x03\x04", 4) || starts("PK\x05\x06", 4) || starts("Rar!", 4) || starts("7z\xBC\xAF", 4) ||
        starts("\x1F\x8B", 2) || starts("\xFD" "7zXZ", 5) || starts("MSCF", 4) || starts("ALZ\x01", 4))
        return L"압축 파일";
    if (starts("MZ", 2) || starts("\x7F" "ELF", 4)) return L"실행 파일";
    if (starts("%PDF", 4) || starts("\xD0\xCF\x11\xE0", 4)) return L"문서 파일 (PDF·워드·한글 등)";
    if (starts("\x89PNG", 4) || starts("\xFF\xD8\xFF", 3) || starts("GIF8", 4) || starts("II*\x00", 4) || starts("MM\x00*", 4))
        return L"그림 파일";
    if (starts("RIFF", 4) || starts("ID3", 3) || starts("OggS", 4) || starts("fLaC", 4) || starts("ftyp", 4, 4) ||
        starts("\x1A\x45\xDF\xA3", 4))
        return L"음악·동영상 파일";
    if (starts("SQLite format 3", 15)) return L"데이터베이스 파일";

    // 3) 내용으로 판단: UTF-16이 아닌데 0 바이트나 제어 문자가 섞여 있으면 텍스트가 아님
    bool utf16 = starts("\xFF\xFE", 2) || starts("\xFE\xFF", 2);
    if (!utf16) {
        size_t chk = rd;
        if (sz.QuadPart > (long long)rd && (chk & 1)) chk--;  // 앞부분만 읽었으면 짝수 길이로
        utf16 = SniffUtf16(buf, (sz.QuadPart == (long long)rd) ? rd : chk) != 0;
    }
    if (!utf16) {
        size_t nul = 0, ctl = 0;
        for (DWORD i = 0; i < rd; ++i) {
            unsigned char c = buf[i];
            if (c == 0) nul++;
            else if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x0C && c != 0x1A && c != 0x1B) ctl++;
        }
        if (nul > 0 || ctl * 100 > rd) return L"텍스트 파일이 아님";
    }
    return L"";
}

static wstring FileName(const wstring& p);
static void ShowNotSupported(const wstring& path, const wstring& why) {
#ifdef MEASURE
    meas::on = false;  // 못 연 파일은 재지 않음
#endif
    wstring m;
    if (why.compare(0, 6, L"파일이 너무") == 0)
        m = L"파일이 너무 커서 열 수 없습니다.\n\n" + FileName(path) + L"\n\n" +
            std::to_wstring(kMaxOpenBytes / (1024 * 1024)) + L"MB까지 열 수 있습니다.";
    else
        m = L"지원하지 않는 파일입니다.\n\n" + FileName(path) + L"\n(" + why +
            L")\n\n텍스트 파일(txt 등)과 EPUB 전자책을 열 수 있습니다.";
    MessageBoxW(g_hwnd, m.c_str(), L"야문 클린리더", MB_ICONWARNING);
}

static bool FileExists(const wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static wstring FileName(const wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == wstring::npos ? p : p.substr(s + 1);
}
// 결과는 원본 확장자(.cap 등)와 상관없이 항상 "이름_정리.txt"
static wstring OutPath(const wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    size_t d = p.find_last_of(L'.');
    if (d == wstring::npos || (s != wstring::npos && d < s)) return p + L"_정리.txt";
    return p.substr(0, d) + L"_정리.txt";
}

// 텍스트가 아닌 파일(이미지, 실행파일 등)인지 대략 판단
static bool LooksBinary(const U32& t) {
    size_t bad = 0, n = t.size() < 200000 ? t.size() : 200000;
    for (size_t i = 0; i < n; ++i) {
        char32_t c = t[i];
        if (c == 0 || c == 0xFFFD || (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x0C && c != 0x1A && c != 0x1B)) bad++;
    }
    return n > 0 && bad > 8 && bad * 100 > n;
}

// EPUB 속 문서가 UTF-8·UTF-16이 아닐 때 (드묾): 텍스트 파일과 같은 자동 판별로 읽음
static U32 DecodeEpubDoc(const std::string& b) { int enc; return Decode(b, enc, 0); }
static bool IsEpubPath(const wstring& p) { return ExtOf(p) == L"epub"; }
// 파일 글 읽기: 텍스트 파일은 인코딩을 판별해 읽고, EPUB은 본문 글을 꺼냄. 실패하면 false, why에 이유
static bool ReadTextOf(const wstring& path, const std::string& bytes, U32& text, int& enc, wstring& why) {
    if (IsEpubPath(path)) {
        U32 t;
        if (!EpubExtract(bytes, t, why, DecodeEpubDoc)) return false;
        text = NormalizeNL(t);
        enc = E_UTF8BOM;  // 저장 인코딩 "원본과 같게": UTF-8 (BOM)
        return true;
    }
    text = NormalizeNL(Decode(bytes, enc));
    if (LooksBinary(text)) { why = L"텍스트 파일이 아님"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// 설정
static int IniGet(const wchar_t* k, int def) { return GetPrivateProfileIntW(L"opt", k, def, g_ini.c_str()); }
static void IniSet(const wchar_t* k, int v) { WritePrivateProfileStringW(L"opt", k, std::to_wstring(v).c_str(), g_ini.c_str()); }

static int CbSel(HWND h) { int i = (int)SendMessageW(h, CB_GETCURSEL, 0, 0); return i < 0 ? 0 : i; }
// 나누기 목록 순서(문장마다 / 긴 문단만 / 나누지 않음) <-> 엔진 값(0 긴 문단만, 1 문장마다, 2 나누지 않음). 서로 역함수
static int SplitMap(int v) { static const int m[3] = {1, 0, 2}; return (v >= 0 && v < 3) ? m[v] : 1; }
static int SplitMode() { return SplitMap(CbSel(g_cbSplit)); }
// 토글 스위치(오너 드로우 버튼): GWLP_USERDATA 비트1 = 켜짐, 비트0 = 마우스 올림
static bool Checked(HWND h) { return (GetWindowLongPtrW(h, GWLP_USERDATA) & 2) != 0; }
static void SetToggle(HWND h, bool on) {
    LONG_PTR v = GetWindowLongPtrW(h, GWLP_USERDATA);
    SetWindowLongPtrW(h, GWLP_USERDATA, on ? (v | 2) : (v & ~2));
    InvalidateRect(h, nullptr, FALSE);
}

static int LongLenValue() {
    wchar_t buf[32]; GetWindowTextW(g_edLong, buf, 32);
    int v = _wtoi(buf);
    if (v < 30) v = 30;
    if (v > 100000) v = 100000;
    return v;
}

static TidyOptions ReadOptions() {
    TidyOptions o;
    o.blankMode = CbSel(g_cbBlank);
    o.joinMode = CbSel(g_cbJoin);
    o.spaceMode = CbSel(g_cbSpace);
    o.splitMode = SplitMode();
    o.longLen = LongLenValue();
    o.dialogueSep = false;
    o.paraSep = CbSel(g_cbParaSep);
    o.sceneMode = CbSel(g_cbScene);
    o.indent = CbSel(g_cbIndent);
    o.collapseSp = Checked(g_chkCollapse);
    o.removeJunk = Checked(g_chkJunk);
    o.quoteMode = CbSel(g_cbQuote);
    o.ellipsisMode = CbSel(g_cbEll);
    o.stripHanja = Checked(g_chkHanja);
    o.fullToHalf = Checked(g_chkHalf);
    o.replaces = g_ruleReplace;
    o.deleteLines = g_ruleDelete;
    o.regexReplaces = g_ruleRegex;
    o.regexDeleteLines = g_ruleRegexDel;
    return o;
}
static int SaveEncoding(int srcEnc) {
    switch (CbSel(g_cbEnc)) {
        case 1: return E_UTF8BOM;
        case 2: return E_UTF8;
        case 3: return E_CP949;
        case 4: return E_UTF16LE;
        default: return srcEnc;
    }
}

// ---------------------------------------------------------------------------
// RichEdit
static wstring RawRichText(HWND h, bool crlf = true) {
    GETTEXTLENGTHEX gl = {(DWORD)((crlf ? GTL_USECRLF : 0) | GTL_PRECISE | GTL_NUMCHARS), 1200};
    LONG len = (LONG)SendMessageW(h, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
    if (len <= 0) return wstring();
    wstring s(len + 1, 0);
    GETTEXTEX gt = {};
    gt.cb = (DWORD)((len + 1) * sizeof(wchar_t));
    gt.flags = crlf ? GT_USECRLF : GT_DEFAULT;
    gt.codepage = 1200;
    LONG got = (LONG)SendMessageW(h, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)&s[0]);
    if (got < 0) got = 0;
    s.resize(got);
    return s;
}
// 화면 표시용 글자 바꿈 (글자 수·위치는 그대로)
// 일부 서식 문자는 글꼴에 모양이 없으면 글 칸에서 빈칸처럼 폭을 차지함.
// 글 칸에는 같은 자리에 폭 없는 문자(U+200B)를 대신 보여 주고, 원래 글은 따로 들고 있다가
// 글을 꺼낼 때(저장·복사·찾기·다시 정리) 원래 글자로 되돌림 → 파일의 글자는 하나도 바뀌지 않음
struct DispMap { wstring orig, disp; bool on = false; std::vector<std::pair<wstring, wstring>> stash; };
static DispMap g_dm[2];  // 0 원본 칸, 1 결과 칸
static DispMap* DmOf(HWND h) { return h == g_src ? &g_dm[0] : h == g_dst ? &g_dm[1] : nullptr; }
static bool DispSubst(const wstring& s, size_t i) {
    unsigned u = (unsigned short)s[i];
    if (u == 0x200D) {  // 이모지 잇기에 쓰인 것은 그대로
        unsigned p = i ? (unsigned short)s[i - 1] : 0;
        return !((p >= 0xDC00 && p <= 0xDFFF) || (p >= 0x2600 && p <= 0x27BF));
    }
    return u == 0x200C || (u >= 0x2060 && u <= 0x2064) || u == 0xFEFF || u == 0x180E;
}
// 글 칸의 지금 글(cur)과 마지막으로 맞춘 표시 글을 비교해, 바뀐 곳만 원래 글에 반영
// caret: 고친 뒤 커서 위치(줄 끝 2글자 기준, 모르면 -1). 같은 표시 문자가 이어진 곳에서 고친 자리를 정확히 가림
static void DmSync(DispMap& m, const wstring& cur, long long caret = -1) {
    if (cur == m.disp) return;
    const wstring& d = m.disp;
    size_t p = 0, n1 = d.size(), n2 = cur.size();
    while (p < n1 && p < n2 && d[p] == cur[p]) ++p;
    size_t q = 0;
    while (q < n1 - p && q < n2 - p && d[n1 - 1 - q] == cur[n2 - 1 - q]) ++q;
    if (m.orig.size() != n1) { m.orig = cur; m.disp = cur; return; }  // 맞지 않으면 지금 글 그대로
    if (caret >= 0 && (size_t)caret <= n2) {  // 커서는 넣은 글의 끝 (지운 경우 지운 자리)
        size_t e = (size_t)caret, il = n2 - p - q;
        if (e >= il && e - il <= p && n2 - e >= q) {
            size_t p2 = e - il, q2 = n2 - e;
            bool ok = p2 + q2 <= n1 && p2 + q2 <= n2;
            for (size_t k = q; ok && k < q2; ++k) ok = d[n1 - 1 - k] == cur[n2 - 1 - k];  // 그 자리로 봐도 뒷부분이 맞아야 함
            if (ok) { p = p2; q = q2; }
        }
    }
    wstring ins = cur.substr(p, n2 - p - q);
    wstring remD = d.substr(p, n1 - p - q), remO = m.orig.substr(p, n1 - p - q);
    if (ins.find(L'\x200B') != wstring::npos)  // 되돌리기(Ctrl+Z) 등으로 지웠던 글이 다시 들어옴: 원래 글자로
        for (auto it = m.stash.rbegin(); it != m.stash.rend(); ++it)
            if (it->first == ins) { ins = it->second; m.stash.erase(std::next(it).base()); break; }
    if (remD != remO) {
        m.stash.push_back({remD, remO});
        if (m.stash.size() > 30) m.stash.erase(m.stash.begin());
    }
    m.orig = m.orig.substr(0, p) + ins + m.orig.substr(n1 - q);
    m.disp = cur;
}
static void DmUpdate(HWND h) {  // 글을 직접 고쳤을 때
    DispMap* m = DmOf(h);
    if (!m || !m->on) return;
    CHARRANGE cr; SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&cr);
    wstring in = RawRichText(h, false);
    long long caret = -1;
    if (cr.cpMin == cr.cpMax && cr.cpMax >= 0 && (size_t)cr.cpMax <= in.size()) {
        caret = cr.cpMax;
        for (LONG i = 0; i < cr.cpMax; ++i) if (in[i] == L'\r') ++caret;
    }
    DmSync(*m, RawRichText(h), caret);
}
static wstring GetRichText(HWND h) {
    wstring cur = RawRichText(h);
    DispMap* m = DmOf(h);
    if (!m || !m->on) return cur;
    DmSync(*m, cur);
    return m->orig;
}
// 글 칸의 선택 부분을 원래 글자로 복사 (Ctrl+C·Ctrl+X·메뉴)
static bool DmCopySel(HWND h, bool cut) {
    DispMap* m = DmOf(h);
    if (!m || (!m->on && !IsRichMode(h))) return false;  // 서식 텍스트 방식의 칸도 글자만 복사 (글꼴 서식이 따라가지 않게)
    CHARRANGE cr; SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&cr);
    if (cr.cpMax <= cr.cpMin) return true;
    if (cut && (GetWindowLongW(h, GWL_STYLE) & ES_READONLY)) return true;
    wstring full = GetRichText(h), in = RawRichText(h, false);  // 칸 안의 위치는 줄 끝이 한 글자(\r)
    auto conv = [&](LONG a) {
        size_t r = 0, lim = std::min((size_t)std::max(0L, a), in.size());
        for (size_t i = 0; i < lim; ++i) if (in[i] == L'\r') ++r;
        return std::min(full.size(), lim + r);
    };
    size_t a = conv(cr.cpMin), b = conv(cr.cpMax);
    wstring w = full.substr(a, b - a);
    if (OpenClipboard(GetParent(h))) {
        EmptyClipboard();
        if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t))) {
            memcpy(GlobalLock(hg), w.c_str(), (w.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(hg);
            if (!SetClipboardData(CF_UNICODETEXT, hg)) GlobalFree(hg);
        }
        CloseClipboard();
    }
    if (cut) SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"");
    return true;
}
static HWND MkRich(int id);
static LRESULT CALLBACK DstProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR);
// 글 칸을 같은 자리·같은 설정으로 새로 만듦
// (긴 글을 지우는 일이 아주 느린 환경이 있어, 많은 글을 바꿀 때는 지우는 대신 새 칸으로 바꿈)
static void RecreateRich(HWND& h) {
    int id = GetDlgCtrlID(h);
    RECT r; GetWindowRect(h, &r);
    MapWindowPoints(nullptr, g_hwnd, (POINT*)&r, 2);
    bool vis = IsWindowVisible(h) != 0;
    bool ro = (GetWindowLongW(h, GWL_STYLE) & ES_READONLY) != 0;
    bool focus = GetFocus() == h;
    HFONT f = (HFONT)SendMessageW(h, WM_GETFONT, 0, 0);
    RECT fr; SendMessageW(h, EM_GETRECT, 0, (LPARAM)&fr);
    HWND n = MkRich(id);
    if (id == ID_DST) SetWindowSubclass(n, DstProc, 0, 0);
    SetWindowPos(n, h, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | (vis ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    if (f) SendMessageW(n, WM_SETFONT, (WPARAM)f, FALSE);
    if (ro) SendMessageW(n, EM_SETREADONLY, TRUE, 0);
    SendMessageW(n, EM_SETRECT, 0, (LPARAM)&fr);
    HWND old = h;
    h = n;
    DestroyWindow(old);
    if (focus) SetFocus(n);
}
static HFONT EditFontOf(HWND h) { return (h == g_dst && g_reading) ? g_readFont : g_edFont; }  // 글 칸에 쓰는 글꼴 (글 칸에 묻지 않음)
// 글꼴에 없는 글자가 있는 글을 "글꼴 지정이 이미 들어간 글(RTF)"로 만들어 한 번에 넣음.
// 묶음마다 하나씩 지정하는 것보다 훨씬 빠름. 한글은 2바이트(CP949) 그대로, 그 밖의 글자(제로폭·간체자 등)는 \uN으로 넣어 글자는 그대로.
// 넣은 글이 원본과 한 글자라도 다르거나 글꼴이 의도대로 들어가지 않으면 false (하나씩 지정하는 방식으로 돌아감)
struct RtfSrc { const std::string* s; size_t pos; };
static DWORD CALLBACK RtfReadCb(DWORD_PTR ck, LPBYTE buf, LONG n, LONG* got) {
    RtfSrc* x = (RtfSrc*)ck;
    size_t m = std::min((size_t)n, x->s->size() - x->pos);
    memcpy(buf, x->s->data() + x->pos, m);
    x->pos += m;
    *got = (LONG)m;
    return 0;
}
static bool g_rtfIn[2];  // 0 원본 칸, 1 결과 칸: 다른 글꼴이 섞인 글을 한 번에 넣기로 열었는지
// 글 칸의 글자 하나에 들어간 글꼴 이름을, 선택(커서)을 옮기지 않고 알아냄 (글 칸의 문서 개체 TOM 사용).
// 선택을 옮기면 글 칸이 커서 자리를 정하려고 글 처음부터 그 자리까지 줄 나눔을 그 자리에서 다 해서,
// 큰 글의 뒤쪽 글자는 몇 초가 걸림. 알아내지 못하면 false (부르는 쪽이 예전 방식으로)
static bool FaceAtNoSel(HWND h, long pos, std::wstring& face) {
    typedef void(WINAPI * SysFreeFn)(BSTR);
    static SysFreeFn sysFree = (SysFreeFn)(void*)GetProcAddress(LoadLibraryW(L"oleaut32.dll"), "SysFreeString");  // Windows 기본 DLL (실행 때 불러옴)
    if (!sysFree) return false;
    static const GUID kIidTextDocument = {0x8CC497C0, 0xA1DF, 0x11CE, {0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D}};  // ITextDocument
    IUnknown* ole = nullptr;
    if (!SendMessageW(h, EM_GETOLEINTERFACE, 0, (LPARAM)&ole) || !ole) return false;
    ITextDocument* doc = nullptr;
    HRESULT hr = ole->QueryInterface(kIidTextDocument, (void**)&doc);
    ole->Release();
    if (FAILED(hr) || !doc) return false;
    bool ok = false;
    ITextRange* r = nullptr;
    if (SUCCEEDED(doc->Range(pos, pos + 1, &r)) && r) {
        ITextFont* f = nullptr;
        if (SUCCEEDED(r->GetFont(&f)) && f) {
            BSTR b = nullptr;
            if (SUCCEEDED(f->GetName(&b)) && b) { face = b; sysFree(b); ok = true; }
            f->Release();
        }
        r->Release();
    }
    doc->Release();
    return ok;
}
// h: 실패해서 비울 때 긴 글이면 새 칸으로 바꾸므로, 부르는 쪽의 칸 핸들도 함께 바뀜
static bool InsertGlyphRtf(HWND& h, const wstring& ins) {
    HFONT bf = EditFontOf(h);
    if (!bf) return false;
    LOGFONTW lf = {};
    GetObjectW(bf, sizeof(lf), &lf);
    const auto& base = CoverageOf(bf);
    auto cp949 = [](const wchar_t* w, int n, std::string& out) {  // 정확히 바꿀 수 있을 때만
        char mb[8];
        BOOL bad = FALSE;
        int k = WideCharToMultiByte(949, WC_NO_BEST_FIT_CHARS, w, n, mb, sizeof(mb), nullptr, &bad);
        if (k <= 0 || bad) return false;
        out.append(mb, k);
        return true;
    };
    // 한글 음절·자모의 CP949 바이트 표 (처음 한 번만 만듦). 글자마다 변환 함수를 부르지 않아 큰 글에서 빠름 (결과 바이트는 같음)
    static std::vector<std::string> hanTab;
    if (hanTab.empty()) {
        hanTab.resize(0xD7A3 - 0xAC00 + 1 + 0x318E - 0x3131 + 1);
        size_t k = 0;
        for (wchar_t c = 0xAC00; c <= 0xD7A3; ++c) cp949(&c, 1, hanTab[k++]);
        for (wchar_t c = 0x3131; c <= 0x318E; ++c) cp949(&c, 1, hanTab[k++]);
    }
    auto hanBytes = [](wchar_t w) -> const std::string* {  // 바꿀 수 없으면 빈 글
        if (w >= 0xAC00 && w <= 0xD7A3) return &hanTab[w - 0xAC00];
        if (w >= 0x3131 && w <= 0x318E) return &hanTab[0xD7A3 - 0xAC00 + 1 + (w - 0x3131)];
        return nullptr;
    };
    std::string face;
    for (const wchar_t* p = lf.lfFaceName; *p; ++p) {
        if (*p == L';' || *p == L'{' || *p == L'}' || *p == L'\\') return false;
        if (*p < 0x80) { face += (char)*p; continue; }
        std::string mb;  // 한글 글꼴 이름: 워드패드와 같은 표준 표기 (\'xx, 한글 문자 집합)
        if (!cp949(p, 1, mb)) return false;
        for (unsigned char ch : mb) { char hx[8]; snprintf(hx, sizeof(hx), "\\'%02x", ch); face += hx; }
    }
    CHARFORMAT2W def = {};  // 빈 칸에 정해 둔 모양 (글자 크기·색)
    def.cbSize = sizeof(def);
    SendMessageW(h, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&def);
    PARAFORMAT2 pf = {};  // 줄 간격 (줄 뒤 여백)
    pf.cbSize = sizeof(pf);
    SendMessageW(h, EM_GETPARAFORMAT, 0, (LPARAM)&pf);

    std::string body;
    body.reserve(ins.size() * 2 + 1024);
    std::wstring expect;  // 글 칸에 들어가야 할 글 (줄 끝은 CR 한 글자)
    expect.reserve(ins.size());
    std::vector<char> used(kFbCount, 0);
    int cur = -1;
    long firstFb = -1, firstBase = -1;
    int firstFbFont = -1;
    for (size_t i = 0; i < ins.size(); ++i) {
        wchar_t c = ins[i];
        if (c == L'\r' || c == L'\n') {
            if (c == L'\r' && i + 1 < ins.size() && ins[i + 1] == L'\n') ++i;
            body += "\\par\n";
            expect += L'\r';
            continue;
        }
        if (c == L'\t') { body += "\\tab "; expect += L'\t'; continue; }
        if (c < 0x20) return false;  // 그 밖의 제어 문자는 그대로 넣는 방식으로
        int f = -1;
        size_t len = 1;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < ins.size() && ins[i + 1] >= 0xDC00 && ins[i + 1] <= 0xDFFF) {  // BMP 밖: 확장 한자 / 그림 문자
            len = 2;
            if (c >= 0xD840 && c <= 0xD87F) f = InstalledFace(L"SimSun");
            if (c >= 0xD83C && c <= 0xD83E) f = InstalledFace(L"Segoe UI Symbol");
        } else if (!SkipForGlyph(c) && !HasGlyph(base, c)) {
            for (int k = 0; k < kFbCount; ++k) {
                const auto* cv = FallbackCoverage(k);
                if (cv && HasGlyph(*cv, c)) { f = k; break; }
            }
        }
        if (f != cur) {
            body += f < 0 ? std::string("\\f0 ") : "\\f" + std::to_string(f + 1) + " ";
            cur = f;
            if (f >= 0) used[f] = 1;
        }
        if (f >= 0 && firstFb < 0) { firstFb = (long)expect.size(); firstFbFont = f; }
        if (f < 0 && firstBase < 0 && !SkipForGlyph(c)) firstBase = (long)expect.size();
        for (size_t u = 0; u < len; ++u) {
            wchar_t w = ins[i + u];
            if (w < 0x80) {
                if (w == L'\\' || w == L'{' || w == L'}') body += '\\';
                body += (char)w;
            } else {
                const std::string* hb = hanBytes(w);
                if (hb && !hb->empty()) body += *hb;
                else body += "\\u" + std::to_string((short)w) + " ";
            }
            expect += w;
        }
        i += len - 1;
    }
    if (firstFb < 0) return false;
    if (!expect.empty() && expect.back() == L'\r') body += "\\par\n";  // RTF의 마지막 문단 기호는 글 칸이 먹으므로, 글이 줄바꿈으로 끝나면 하나 더
    // 글자 크기: RTF는 반 포인트 단위라, 화면에서 지금과 같은 픽셀 크기가 되는 값을 고름 (글 전체 크기를 다시 맞추지 않아도 되게)
    int halfPt = (def.yHeight + 5) / 10;
    for (int d : {0, -1, 1, -2, 2})
        if (MulDiv((halfPt + d) * 10, g_dpi, 1440) == MulDiv(def.yHeight, g_dpi, 1440)) { halfPt += d; break; }
    std::string rtf = "{\\rtf1\\ansi\\ansicpg949\\deff0\\uc0{\\fonttbl{\\f0\\fnil\\fcharset129 " + face + ";}";
    for (int k = 0; k < kFbCount; ++k)
        if (used[k]) rtf += "{\\f" + std::to_string(k + 1) + "\\fnil\\fcharset" + std::to_string((int)kFallbackFonts[k].charset) + " " + std::string(kFallbackFonts[k].face, kFallbackFonts[k].face + wcslen(kFallbackFonts[k].face)) + ";}";  // 후보 이름은 모두 영문
    rtf += "}{\\colortbl;\\red" + std::to_string(GetRValue(def.crTextColor)) + "\\green" + std::to_string(GetGValue(def.crTextColor)) + "\\blue" +
           std::to_string(GetBValue(def.crTextColor)) + ";}\\pard\\sa" + std::to_string(pf.dySpaceAfter) +
           (pf.bLineSpacingRule == 4 && pf.dyLineSpacing > 0 ? "\\sl-" + std::to_string(pf.dyLineSpacing) + "\\slmult0" : std::string()) +
           "\\plain" + (lf.lfWeight >= FW_SEMIBOLD ? "\\b" : "") + "\\f0\\fs" +
           std::to_string(halfPt) + "\\cf1 " + body + "}";
    MEAS_MARK(L"   └ RTF 글 만들기 (" + meas::Num((long long)rtf.size()) + L"바이트)");
    RtfSrc src = {&rtf, 0};
    EDITSTREAM es = {(DWORD_PTR)&src, 0, RtfReadCb};
    SendMessageW(h, EM_STREAMIN, SF_RTF, (LPARAM)&es);
    MEAS_MARK(L"   └ 글 칸이 RTF 읽어 넣기");
    auto undo = [&]() {  // 실패: 비우고 원래 모양으로 (부르는 쪽이 보통 방식으로 다시 넣음)
        if ((h == g_src || h == g_dst) && RichLen(h) > 20000) {  // 긴 글은 지우는 일이 아주 느릴 수 있어 새 칸으로 바꿈 (여는 때와 같이)
            HWND& g = (h == g_src) ? g_src : g_dst;
            RecreateRich(g);
            h = g;
            SendMessageW(h, WM_SETREDRAW, FALSE, 0);
            SendMessageW(h, EM_SETTEXTMODE, TM_RICHTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE, 0);
        } else {
            SETTEXTEX st = {ST_DEFAULT, 1200};
            SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)L"");
        }
        SendMessageW(h, WM_SETFONT, (WPARAM)bf, FALSE);
        ApplyEditLook(h);
        SendMessageW(h, WM_SETREDRAW, FALSE, 0);  // ApplyEditLook이 다시 그리기를 켜므로 넣는 동안은 다시 끔
        return false;
    };
    if (es.dwError || RawRichText(h, false) != expect) return undo();
    MEAS_MARK(L"   └ 넣은 글이 원본과 같은지 확인 (" + meas::Num((long long)expect.size()) + L"자)");
    auto fmtAt = [&](long pos) {
        CHARRANGE pr = {pos, pos + 1};
        SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&pr);
        CHARFORMAT2W c = {};
        c.cbSize = sizeof(c);
        SendMessageW(h, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&c);
        return c;
    };
    // 처음 다른 글꼴 글자: 글 뒤쪽에 있을 수 있으므로 선택을 옮기지 않고 글꼴만 물음 (못 하면 예전처럼 선택해서)
    std::wstring fbFace;
    bool viaTom = FaceAtNoSel(h, firstFb, fbFace);
    CHARFORMAT2W a = {};
    if (!viaTom) { a = fmtAt(firstFb); fbFace = a.szFaceName; }
    MEAS_MARK(L"   └ 처음 다른 글꼴 글자의 글꼴 확인 (글의 " + std::to_wstring(expect.empty() ? 0 : (int)((double)firstFb * 100 / expect.size())) + L"% 지점, " + (viaTom ? L"선택 없이)" : L"선택해서)"));
    if (lstrcmpiW(fbFace.c_str(), kFallbackFonts[firstFbFont].face) != 0) return undo();
    if (firstBase >= 0) {
        CHARFORMAT2W b = fmtAt(firstBase);
        MEAS_MARK(L"   └ 처음 보통 글자의 글꼴 확인 (글의 " + std::to_wstring(expect.empty() ? 0 : (int)((double)firstBase * 100 / expect.size())) + L"% 지점)");
        if (lstrcmpiW(b.szFaceName, lf.lfFaceName) != 0) return undo();
        // 글자 크기·색은 글 전체에 같게 넣었으므로(다른 글꼴 글자는 글꼴만 바뀜) 앞쪽 보통 글자로 확인
        if (viaTom) a = b;
    } else if (viaTom) {
        a = fmtAt(firstFb);  // 보통 글자가 없는 글: 다른 글꼴 글자가 맨 앞쪽
    }
    if (MulDiv(a.yHeight, g_dpi, 1440) != MulDiv(def.yHeight, g_dpi, 1440) || a.crTextColor != def.crTextColor ||
        (a.dwEffects & CFE_AUTOCOLOR) != (def.dwEffects & CFE_AUTOCOLOR)) {
        CHARFORMAT2W fix = {};  // (드묾) 화면 크기·색이 다르면 크기·색만 맞춤 (글꼴은 그대로)
        fix.cbSize = sizeof(fix);
        fix.dwMask = CFM_SIZE | CFM_COLOR;
        fix.yHeight = def.yHeight;
        fix.crTextColor = def.crTextColor;
        fix.dwEffects = def.dwEffects & CFE_AUTOCOLOR;
        SendMessageW(h, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&fix);
        MEAS_MARK(L"   └ 글 전체 크기·색 다시 맞추기 (드문 경우)");
    }
    return true;
}
static void SetRichText(HWND h, const wstring& s) {
#ifdef MEASURE
    const wstring mp = h == g_src ? L"[원본 칸] " : h == g_dst ? L"[결과 칸] " : L"[다른 칸] ";
    MEAS_MARK(mp + L"글 넣기 전 준비");
#endif
    if (h == g_src || h == g_dst) {
        HWND& g = (h == g_src) ? g_src : g_dst;
        if (RichLen(g) > 20000) RecreateRich(g);
        h = g;
    }
    MEAS_MARK(mp + L"예전 글 칸 바꾸기(새 칸 만들기)");
    g_loading = true;
    SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    SETTEXTEX st = {ST_DEFAULT, 1200};
    // 글꼴에 없는 글자(간체자 등)가 있는 글만 서식 텍스트 방식으로 열어 그 글자에만 다른 글꼴을 지정함 (보통 글은 예전과 똑같이)
    // "{\rtf"로 시작하는 글은 서식으로 읽히지 않게 늘 일반 텍스트 방식
    bool needFb = (h == g_src || h == g_dst) && FontLacksGlyphs(EditFontOf(h), s) && _wcsnicmp(s.c_str(), L"{\\rtf", 5) != 0;
    MEAS_MARK(mp + L"글꼴에 없는 글자 검사");
    if (!s.empty()) MEAS_NOTE(mp + L"글꼴에 없는 글자: " + (needFb ? L"있음" : L"없음 → 보통 방식"));
    if (h == g_src || h == g_dst) SetAutoFont(h, false);  // 예전과 같이 자동 글꼴 바꿈은 끔
    if (h == g_src || h == g_dst) {  // 빈 상태에서 모양(색·줄 간격)을 정한 뒤 글을 넣음: 긴 글을 다시 배치하지 않아 빠름
        SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)L"");
        if (needFb != IsRichMode(h)) {  // 방식은 빈 칸에서만 바꿀 수 있음
            SendMessageW(h, EM_EMPTYUNDOBUFFER, 0, 0);
            SendMessageW(h, EM_SETTEXTMODE, (needFb ? TM_RICHTEXT : TM_PLAINTEXT) | TM_MULTILEVELUNDO | TM_MULTICODEPAGE, 0);
            SendMessageW(h, WM_SETFONT, (WPARAM)EditFontOf(h), FALSE);
        }
        ApplyEditLook(h);
        g_loading = true;
        SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    }
    MEAS_MARK(mp + L"빈 칸에 모양(색·줄 간격) 정하기");
    DispMap* dm = DmOf(h);
    wstring disp;
    if (dm) {
        dm->on = false;
        for (size_t i = 0; h == g_dst && i < s.size(); ++i)  // 원본 칸은 원문 그대로 보여 줌 (결과 칸만 바꿔 보임)
            if (DispSubst(s, i)) { if (!dm->on) { disp = s; dm->on = true; } disp[i] = 0x200B; }
        dm->stash.clear();
        if (dm->on) { dm->orig = s; dm->disp = disp; }
        else { dm->orig.clear(); dm->disp.clear(); }
    }
    MEAS_MARK(mp + L"보이지 않는 서식 문자 표시용 바꿈");
    if (dm && dm->on) MEAS_NOTE(mp + L"보이지 않는 서식 문자(U+200C 등): 있음 → 표시용 사본 만들고 넣은 뒤 다시 확인");
    bool rtfDone = false;  // 글꼴 지정이 들어간 글로 한 번에 넣었는지
    {
        bool os = g_syncing; g_syncing = true;
        if (needFb) rtfDone = InsertGlyphRtf(h, dm && dm->on ? disp : s);
        g_syncing = os;
    }
    if (needFb) MEAS_MARK(mp + L"서식(RTF) 방식으로 글 넣기" + (rtfDone ? wstring() : wstring(L" (실패)")));
    if (!rtfDone) SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)(dm && dm->on ? disp.c_str() : s.c_str()));
    if (!rtfDone) MEAS_MARK(mp + L"글 넣기 (" + meas::Num((long long)s.size()) + L"자)");
    if (dm && dm->on) {  // 칸이 글을 바꿔 넣었으면: 길이가 같으면 표시 글만 맞추고, 다르면 바꿈 없이 원래 글을 넣음
        wstring cur = RawRichText(h);
        if (cur != dm->disp) {
            if (cur.size() == dm->disp.size()) dm->disp = cur;
            else {
                dm->on = false; dm->orig.clear(); dm->disp.clear();
                SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)s.c_str());
                rtfDone = false;
            }
        }
        MEAS_MARK(mp + L"넣은 글 다시 꺼내 확인");
    }
    if (needFb && !rtfDone) {
        // (한 번에 넣기를 못 했을 때) 없는 글자 묶음마다 글꼴을 지정. 이것도 못 하면 Windows 자동 글꼴 바꿈으로 다시 넣음
        bool os = g_syncing; g_syncing = true;
        if (!ApplyGlyphFallback(h)) {
            std::wstring cur = RawRichText(h);
            MEAS_MARK(mp + L"없는 글자에 글꼴 하나씩 지정 (못 함)");
            // 묶음마다 지정도 못 했으면 Windows 자동 글꼴 바꿈으로 다시 넣음
            // 긴 글을 지우는 일은 아주 느릴 수 있으므로, 비울 때는 여는 때와 같이 새 칸으로 바꿈
            if (h == g_src || h == g_dst) {
                HWND& g = (h == g_src) ? g_src : g_dst;
                if (RichLen(g) > 20000) {
                    RecreateRich(g);
                    h = g;
                    SendMessageW(h, WM_SETREDRAW, FALSE, 0);
                    SendMessageW(h, EM_SETTEXTMODE, TM_RICHTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE, 0);
                    SendMessageW(h, WM_SETFONT, (WPARAM)EditFontOf(h), FALSE);
                    SetAutoFont(h, false);
                    ApplyEditLook(h);
                    SendMessageW(h, WM_SETREDRAW, FALSE, 0);
                } else {
                    SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)L"");
                }
            }
            SetAutoFont(h, true);
            SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)cur.c_str());
        }
        g_syncing = os;
        MEAS_MARK(mp + L"없는 글자에 글꼴 하나씩 지정");
    }
    if (h == g_src || h == g_dst) g_rtfIn[h == g_dst ? 1 : 0] = rtfDone;
    SendMessageW(h, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessageW(h, EM_SETSEL, 0, 0);
    SendMessageW(h, EM_SCROLLCARET, 0, 0);
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(h, nullptr, TRUE);
    g_loading = false;
    MEAS_MARK(mp + L"마무리(커서를 맨 앞으로)");
}
// 테마를 바꿀 때 글자색 바꾸기. 다른 글꼴이 섞인 글(한 번에 넣기로 연 글)은 글꼴 구간이 많아 글 전체에 색을 다시 칠하면 느리므로,
// 빈 칸에 새 모양을 정한 뒤 지금 글을 새 색으로 한 번에 다시 넣음 (글·보던 위치·선택은 그대로)
static void RecolorEdit(HWND h) {
    if (!h) return;
    if (!(h == g_src || h == g_dst) || !g_rtfIn[h == g_dst ? 1 : 0] || !IsRichMode(h) || RichLen(h) == 0) { ApplyEditLook(h); return; }
    bool ol = g_loading, os = g_syncing;
    g_loading = true; g_syncing = true;  // 바뀜·선택 알림 무시
    wstring cur = RawRichText(h, false);
    long top = TopChar(h);
    CHARRANGE cr; SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&cr);
    SETTEXTEX st = {ST_DEFAULT, 1200};
    if (RichLen(h) > 20000) {  // 긴 글은 여는 때와 같이 지우는 대신 새 칸으로 바꿈
        HWND& g = (h == g_src) ? g_src : g_dst;
        RecreateRich(g);
        h = g;
        SendMessageW(h, WM_SETREDRAW, FALSE, 0);
        SendMessageW(h, EM_SETTEXTMODE, TM_RICHTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)EditFontOf(h), FALSE);
    } else {
        SendMessageW(h, WM_SETREDRAW, FALSE, 0);
        SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)L"");
    }
    ApplyEditLook(h);
    g_loading = true; g_syncing = true;
    SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    if (!InsertGlyphRtf(h, cur)) {  // (드묾) 못 넣으면 여는 때와 같은 차례로
        g_rtfIn[h == g_dst ? 1 : 0] = false;
        SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)cur.c_str());
        if (!ApplyGlyphFallback(h)) {
            SetAutoFont(h, true);
            SendMessageW(h, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)cur.c_str());
        }
    }
    SendMessageW(h, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);
    ScrollToChar(h, top);
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(h, nullptr, TRUE);
    g_loading = ol; g_syncing = os;
}

// ---------------------------------------------------------------------------
// 위치 연동
struct CpConv {  // u32 인덱스 <-> RichEdit 글자 위치
    std::vector<int> pre;  // pre[i] = i 앞의 보조평면 글자 수 (없으면 비어 있음)
    void build(const U32& t) {
        pre.clear();
        bool any = false;
        for (char32_t c : t) if (c >= 0x10000) { any = true; break; }
        if (!any) return;
        pre.resize(t.size() + 1);
        int k = 0;
        for (size_t i = 0; i < t.size(); ++i) { pre[i] = k; if (t[i] >= 0x10000) k++; }
        pre[t.size()] = k;
    }
    int toCp(int i) const { return pre.empty() ? i : i + pre[i]; }
    int toIdx(int cp) const {
        if (pre.empty()) return cp;
        int lo = 0, hi = (int)pre.size() - 1;
        while (lo < hi) { int mid = (lo + hi + 1) / 2; if (mid + pre[mid] <= cp) lo = mid; else hi = mid - 1; }
        return lo;
    }
};
static CpConv g_cvSrc, g_cvOut;

static void BuildMaps() {
    g_srcToOut.assign(g_text.size() + 1, -1);
    for (size_t i = 0; i < g_outToSrc.size(); ++i) {
        int sidx = g_outToSrc[i];
        if (sidx >= 0 && sidx <= (int)g_text.size() && g_srcToOut[sidx] < 0) g_srcToOut[sidx] = (int)i;
    }
    int next = (int)g_out.size();
    for (int j = (int)g_srcToOut.size() - 1; j >= 0; --j) {
        if (g_srcToOut[j] < 0) g_srcToOut[j] = next; else next = g_srcToOut[j];
    }
    g_cvSrc.build(g_text);
    g_cvOut.build(g_out);
}

static int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// from 창에서 fromCp 줄이 보이던 높이에 맞춰 to 창의 toCp 줄이 오도록 스크롤
static void AlignScroll(HWND from, int fromCp, HWND to, int toCp) {
    int lf = (int)SendMessageW(from, EM_EXLINEFROMCHAR, 0, fromCp);
    int ff = (int)SendMessageW(from, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lt = (int)SendMessageW(to, EM_EXLINEFROMCHAR, 0, toCp);
    int ft = (int)SendMessageW(to, EM_GETFIRSTVISIBLELINE, 0, 0);
    int offset = lf - ff;
    RECT rc; SendMessageW(to, EM_GETRECT, 0, (LPARAM)&rc);
    TEXTMETRICW tm = {};
    HDC dc = GetDC(to); HGDIOBJ old = SelectObject(dc, g_edFont); GetTextMetricsW(dc, &tm); SelectObject(dc, old); ReleaseDC(to, dc);
    int visible = tm.tmHeight > 0 ? (rc.bottom - rc.top) / tm.tmHeight : 20;
    if (offset < 0 || offset >= visible) offset = visible / 3;
    int want = lt - offset;
    if (want < 0) want = 0;
    SendMessageW(to, EM_LINESCROLL, 0, want - ft);
}

static void SelectIn(HWND h, int cpA, int cpB) {
    CHARRANGE cr = {cpA, cpB};
    SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);
}

static bool SyncReady() {
    return !g_reading && Checked(g_chkSync) && !g_syncing && !g_loading && !g_dstDirty && !g_srcPending && !g_out.empty() &&
           g_srcToOut.size() == g_text.size() + 1;
}

// 원본에서 클릭/선택 -> 결과의 해당 부분 선택 + 같은 높이로 스크롤
static void SyncFromSrc(const CHARRANGE& r) {
    if (!SyncReady()) return;
    int n = (int)g_text.size();
    int sA = Clamp(g_cvSrc.toIdx(r.cpMin), 0, n);
    int sB = Clamp(g_cvSrc.toIdx(r.cpMax), 0, n);
    int oA, oB;
    if (sA == sB) {
        // 줄 끝을 클릭하면 커서가 줄바꿈 위치에 있으므로 그 줄의 마지막 글자로 봄
        if (sA > 0 && (sA >= n || g_text[sA] == '\n') && g_text[sA - 1] != '\n') --sA;
        // 클릭만 했으면 결과 쪽 문단 전체를 표시
        int o = Clamp(g_srcToOut[sA], 0, (int)g_out.size());
        if (o < (int)g_out.size() && g_out[o] == '\n') { while (o < (int)g_out.size() && g_out[o] == '\n') ++o; }
        oA = o; oB = o;
        while (oA > 0 && g_out[oA - 1] != '\n') --oA;
        while (oB < (int)g_out.size() && g_out[oB] != '\n') ++oB;
    } else {
        oA = Clamp(g_srcToOut[sA], 0, (int)g_out.size());
        oB = Clamp(g_srcToOut[sB > 0 ? sB - 1 : 0] + 1, oA, (int)g_out.size());
    }
    g_syncing = true;
    SelectIn(g_dst, g_cvOut.toCp(oA), g_cvOut.toCp(oB));
    AlignScroll(g_src, r.cpMin, g_dst, g_cvOut.toCp(oA));
    g_syncing = false;
}

// 결과에서 클릭/선택 -> 원본의 해당 부분 선택 + 같은 높이로 스크롤
static void SyncFromDst(const CHARRANGE& r) {
    if (!SyncReady()) return;
    int n = (int)g_out.size();
    if (n == 0) return;
    int oA = Clamp(g_cvOut.toIdx(r.cpMin), 0, n - 1);
    int oB = Clamp(g_cvOut.toIdx(r.cpMax), 0, n);
    if (oA == oB || oB <= oA) {
        while (oA < n && g_out[oA] == '\n' && (oA == 0 || g_out[oA - 1] == '\n')) ++oA;  // 빈 줄이면 다음 문단
        if (oA >= n) return;
        oB = oA;
        while (oA > 0 && g_out[oA - 1] != '\n') --oA;
        while (oB < n && g_out[oB] != '\n') ++oB;
        if (oA == oB) return;
    }
    int sA = Clamp(g_outToSrc[oA], 0, (int)g_text.size());
    int sB = Clamp(g_outToSrc[oB - 1] + 1, sA, (int)g_text.size());
    g_syncing = true;
    SelectIn(g_src, g_cvSrc.toCp(sA), g_cvSrc.toCp(sB));
    AlignScroll(g_dst, r.cpMin, g_src, g_cvSrc.toCp(sA));
    g_syncing = false;
}

// ---------------------------------------------------------------------------
static RECT g_rcStatus, g_rcHeader;
static void Layout();

static bool g_statusOk = false;      // 상태줄 글을 초록색(완료)으로
static bool g_savedFlash = false;    // [저장] 버튼을 잠깐 "저장됨"으로
static wstring g_lastSaved;          // 이번에 이 프로그램이 저장한 파일 (다시 저장할 때는 묻지 않음)
static void SetStatus(const wstring& msg) {
    g_statusMsg = msg;
    g_statusOk = false;
    InvalidateRect(g_hwnd, &g_rcStatus, FALSE);
}

static void UpdateTitle() {
    wstring t = L"야문 클린리더";
#ifdef MEASURE
    t += L" [측정판]";
#endif
    if (!g_path.empty()) t = g_path + L" - " + t;  // 제목 표시줄에는 전체 위치
    SetWindowTextW(g_hwnd, t.c_str());
    InvalidateRect(g_hwnd, &g_rcHeader, FALSE);
}

// ---------------------------------------------------------------------------
// 큰 글: 화면에는 앞부분만, 정리·저장은 전체
static bool g_preview = false;       // 화면에 앞부분만 보이는 중
static size_t g_prevSrcEnd = 0;      // 원본 칸에 보이는 글자 수
static std::vector<wstring> g_mergeFiles;  // 여러 파일을 합쳐 연 경우 그 파일들 (g_path는 가상의 이름)
static std::vector<size_t> g_mergeBounds;  // 합친 글에서 각 파일이 시작하는 위치 (파일마다 따로 판단해 정리)
static bool g_epub = false;                 // 지금 글이 EPUB 전자책에서 꺼낸 글
static const U32 kMergeSep = U"\n\n\n";   // 합칠 때 파일 사이

// 합친 글은 파일마다 형식(빈 줄, 고정폭 등)이 다를 수 있으므로 파일별로 정리해 이어 붙임
static U32 TidyParts(const U32& text, const std::vector<size_t>& bounds, const TidyOptions& o, TidyReport* rep, std::vector<int>* map) {
    if (bounds.size() < 2) return TidyProcess(text, o, rep, map);
    U32 out;
    TidyReport sum;
    if (map) map->clear();
    for (size_t k = 0; k < bounds.size(); ++k) {
        size_t a = bounds[k], b = k + 1 < bounds.size() ? bounds[k + 1] - kMergeSep.size() : text.size();
        if (a > text.size()) break;
        if (b > text.size() || b < a) b = text.size();
        TidyReport r;
        std::vector<int> m;
        U32 part = TidyProcess(text.substr(a, b - a), o, &r, map ? &m : nullptr);
        while (!part.empty() && part.back() == '\n') { part.pop_back(); if (map && !m.empty()) m.pop_back(); }
        if (!out.empty()) {
            for (size_t q = 0; q < kMergeSep.size(); ++q) { out += '\n'; if (map) map->push_back((int)a); }
        }
        out += part;
        if (map) for (int v : m) map->push_back(v + (int)a);
        sum.inLines += r.inLines; sum.textLines += r.textLines; sum.outParas += r.outParas;
        sum.doubleSpaced |= r.doubleSpaced; sum.listLike |= r.listLike; if (r.wrapWidth) { sum.wrapWidth = r.wrapWidth; sum.wrapCut = r.wrapCut; }
        sum.indentMarks |= r.indentMarks; sum.joined |= r.joined; sum.longestLine = std::max(sum.longestLine, r.longestLine);
        sum.spaceFixes += r.spaceFixes; sum.pageNumLines += r.pageNumLines; sum.htmlFixes += r.htmlFixes; sum.letterSpaced |= r.letterSpaced;
        sum.wideGapBreaks += r.wideGapBreaks; sum.junkLines += r.junkLines; sum.charFixes += r.charFixes; sum.replaceHits += r.replaceHits;
        sum.jamoFixes += r.jamoFixes; sum.chapterBreaks += r.chapterBreaks; sum.sceneBreaks += r.sceneBreaks; sum.unevenGaps |= r.unevenGaps;
    }
    out += '\n';
    if (map) map->push_back((int)text.size());
    if (rep) *rep = sum;
    return out;
}
static bool g_busy = false, g_again = false;  // 뒤에서 정리하는 중 / 끝나면 다시 정리
static int g_gen = 0;
#define WM_APP_PROCDONE (WM_APP + 20)

static size_t PreviewCut(const U32& t, size_t want) {
    if (t.size() <= want) return t.size();
    size_t cut = want, lim = std::min(t.size(), want + 20000);
    while (cut < lim && t[cut] != '\n') ++cut;  // 줄 끝에서 자름 (아주 긴 줄이면 그냥 자름)
    return cut < lim ? cut : want;
}
static wstring PreviewNote(size_t total) {
    return L"\r\n\r\n──────  여기까지 미리보기 · 전체 약 " + std::to_wstring((total + 5000) / 10000) +
           L"만 자는 저장할 때 모두 정리됩니다  ──────\r\n";
}
// 원본 칸 채우기
// 읽기 모드에서는 원본 칸이 보이지 않으므로 비워만 두고, 정리 모드로 바꿀 때 채움 (큰 글을 여는 시간 단축)
static bool g_srcStale = false;  // 원본 칸을 아직 채우지 않음 (읽기 모드에서 연 글)
static void FillSource() {
    wstring w = U32ToW(g_preview ? g_text.substr(0, g_prevSrcEnd) : g_text, true);
    if (g_preview) w += PreviewNote(g_text.size());
    SetRichText(g_src, w);
    g_srcStale = false;
}
static void ShowSource() {
    SaveReadPos();      // 앞 글의 읽던 곳
    g_newDoc = true;
    g_preview = g_text.size() > kPreviewChars + kPreviewChars / 4;
    g_prevSrcEnd = g_preview ? PreviewCut(g_text, kPreviewChars) : g_text.size();
    if (g_reading) {
        SetRichText(g_src, L"");
        g_srcStale = !g_text.empty();
    } else {
        FillSource();
    }
    SendMessageW(g_src, EM_SETREADONLY, g_preview ? TRUE : FALSE, 0);
    SendMessageW(g_dst, EM_SETREADONLY, (g_preview || g_reading) ? TRUE : FALSE, 0);
}

struct ProcJob {
    std::shared_ptr<U32> text;
    std::vector<size_t> bounds;
    TidyOptions o;
    int gen = 0;
    U32 out;
    std::vector<int> map;
    TidyReport r;
    long long ms = 0;
};
static void AfterTextChange();

// 결과 창의 맨 위 글자 위치
static long GlobalTop() { return TopChar(g_dst); }
static void GoToGlobal(long idx) { ScrollToChar(g_dst, idx < 0 ? 0 : idx); }
// 결과 창에 글 넣기: 보통은 전체, 큰 글은 정리 모드에서 앞부분만 (읽기 모드는 큰 글도 처음부터 끝까지 한 번에)
static void ShowResultText() {
    if (!g_preview || g_reading) {
        wstring w = U32ToW(g_out, true);
        MEAS_MARK(L"결과 글을 화면용 글자로 바꾸기");
        SetRichText(g_dst, w);
    } else {
        size_t oc = g_prevSrcEnd < g_srcToOut.size() ? (size_t)g_srcToOut[g_prevSrcEnd] : g_out.size();
        if (oc > g_out.size()) oc = g_out.size();
        size_t lim = std::min(g_out.size(), oc + 20000);
        while (oc < lim && g_out[oc] != '\n') ++oc;
        SetRichText(g_dst, U32ToW(g_out.substr(0, oc), true) + PreviewNote(g_out.size()));
    }
}
static void FinishReprocess(ProcJob& j) {
    long keepTop = g_newDoc ? -1 : GlobalTop(), keepOutLen = (long)g_out.size();
    g_out.swap(j.out);
    g_outToSrc.swap(j.map);
    const TidyReport& r = j.r;
    const TidyOptions& o = j.o;
    BuildMaps();
    MEAS_MARK(L"원본↔결과 위치 표 만들기");
    long newLen = (long)g_out.size();
    auto scaled = [&](long idx, long oldLen) { return (oldLen > 0 && oldLen != newLen) ? (long)((double)idx * newLen / oldLen) : idx; };
    bool restoring = g_newDoc && g_wantIdx > 0;
    long target = restoring ? scaled(g_wantIdx, g_wantLen) : keepTop > 0 ? scaled(keepTop, keepOutLen) : 0;
    ShowResultText();
    g_dstDirty = false;
    MEAS_NOTE(L"글자 수: 원문 " + meas::Num((long long)g_text.size()) + L"자 → 결과 " + meas::Num((long long)g_out.size()) + L"자" +
              (g_preview && !g_reading ? L" (정리 모드: 앞부분만 화면에)" : L" (화면에 전체)"));

    g_metaSrc = g_epub ? wstring(L"EPUB") : wstring(kEncName[g_enc]);
    {   // 연 파일의 크기 (여러 파일을 합쳐 열었으면 합친 크기, 붙여넣은 글은 없음)
        long long bytes = 0;
        auto add = [&](const wstring& f) {
            WIN32_FILE_ATTRIBUTE_DATA fa;
            if (GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &fa)) bytes += ((long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
        };
        if (!g_mergeFiles.empty()) for (auto& f : g_mergeFiles) add(f);
        else if (!g_path.empty()) add(g_path);
        g_sizeText = bytes > 0 ? SizeText(bytes) : wstring();
        if (bytes > 0) g_metaSrc += L"  ·  " + g_sizeText;
    }
    g_metaSrc += L"  ·  " + std::to_wstring(r.inLines) + L"줄";
    g_metaDst = std::to_wstring(r.outParas) + L"문단  ·  " +
                (j.ms >= 1000 ? std::to_wstring(j.ms / 1000) + L"." + std::to_wstring(j.ms % 1000 / 100) + L"초" : std::to_wstring(j.ms) + L"ms");
    if (g_preview) { g_metaSrc += L"  ·  앞부분만 표시"; g_metaDst += L"  ·  저장은 전체"; }  // 크기가 함께 보이므로 "큰 파일이라"는 줄임
    g_chips.clear();
    if (!g_mergeFiles.empty()) g_chips.push_back(std::to_wstring(g_mergeFiles.size()) + L"개 파일 합침");
    if (r.wrapWidth) g_chips.push_back(r.wrapCut ? L"고정폭 " + std::to_wstring(r.wrapWidth) + L"칸 줄바꿈 복원"
                                                 : L"단어 단위 줄바꿈 복원");
    if (r.doubleSpaced) g_chips.push_back(L"줄마다 빈 줄 제거");
    if (r.indentMarks) g_chips.push_back(L"들여쓰기 문단 인식");
    if (r.textLines <= 3 && r.longestLine > 500) g_chips.push_back(L"한 줄 긴 글 나눔");
    if (r.listLike) g_chips.push_back(L"목록형 글: 줄 그대로");
    if (r.joined && !r.wrapWidth) g_chips.push_back(L"끊긴 줄 합침");
    if (r.wideGapBreaks) g_chips.push_back(L"두 칸 공백 문단 " + std::to_wstring(r.wideGapBreaks) + L"곳");
    if (r.letterSpaced) g_chips.push_back(L"자간 띄운 글 복원");
    if (r.spaceFixes) g_chips.push_back(L"공백·문장부호 " + std::to_wstring(r.spaceFixes) + L"곳");
    if (r.pageNumLines) g_chips.push_back(L"쪽 번호 " + std::to_wstring(r.pageNumLines) + L"줄 제거");
    if (r.htmlFixes) g_chips.push_back(L"HTML 찌꺼기 " + std::to_wstring(r.htmlFixes) + L"곳");
    if (r.junkLines) g_chips.push_back(L"광고·주소·머리글 " + std::to_wstring(r.junkLines) + L"줄 제거");
    if (r.jamoFixes) g_chips.push_back(L"분리된 자모 " + std::to_wstring(r.jamoFixes) + L"자 결합");
    if (r.chapterBreaks) g_chips.push_back(L"회차 제목 " + std::to_wstring(r.chapterBreaks) + L"곳 분리");
    if (r.charFixes) g_chips.push_back(L"특수문자 " + std::to_wstring(r.charFixes) + L"곳");
    if (r.replaceHits) g_chips.push_back(L"내 규칙 " + std::to_wstring(r.replaceHits) + L"곳");
    if (r.sceneBreaks && o.sceneMode != 2) g_chips.push_back(L"장면 전환 " + std::to_wstring(r.sceneBreaks) + L"곳 유지");
    if (r.unevenGaps) g_chips.push_back(L"제각각인 문단 간격 통일");
    MEAS_MARK(L"정보 글 만들기");
    Layout();
    MEAS_MARK(L"창 배치(Layout · 여백 다시 지정)");
    {   // 보던 곳 유지 · 다시 연 파일은 지난번 읽던 곳으로
        if (g_newDoc) g_pendingTop = -1;
        if (target > 0) {
            if (g_preview && !g_reading && target >= RichLen(g_dst) - 1) {  // 정리 모드에서 앞부분 밖: 읽기 모드에서 감
                g_pendingTop = target;
                if (restoring) g_restoreNote = L"   ·   지난번 읽던 곳은 읽기 모드에서 이어 볼 수 있습니다";
            } else {
                GoToGlobal(target);
                if (restoring) g_restoreNote = L"   ·   지난번 읽던 곳(" + std::to_wstring(ReadPercent()) + L"%)으로 이동했습니다";
            }
        }
        g_wantIdx = -1;
        g_newDoc = false;
        g_posPath = g_mergeFiles.empty() ? g_path : wstring();
        g_readPct = -1;
    }
    MEAS_MARK(target > 0 ? L"지난번 읽던 곳으로 이동" : L"맨 처음으로 (이동 없음)");
    if (MEAS_ON() && target > 0) MEAS_NOTE(L"읽던 곳 복원: 글의 약 " + std::to_wstring(newLen > 0 ? (int)((double)target * 100 / newLen) : 0) + L"% 지점");
    InvalidateRect(g_hwnd, nullptr, FALSE);
    if (MEAS_ON()) PostMessageW(g_hwnd, WM_APP_MEAS, 0, 0);
}

static DWORD WINAPI ProcThread(LPVOID p) {
    ProcJob* j = (ProcJob*)p;
    auto t0 = std::chrono::steady_clock::now();
    j->out = TidyParts(*j->text, j->bounds, j->o, &j->r, &j->map);
    j->ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    j->text.reset();
    if (!PostMessageW(g_hwnd, WM_APP_PROCDONE, 0, (LPARAM)j)) delete j;
    return 0;
}
static void StartJob() {
    ProcJob* j = new ProcJob;
    j->text = std::make_shared<U32>(g_text);
    j->bounds = g_mergeBounds;
    j->o = ReadOptions();
    j->gen = g_gen;
    g_busy = true;
    g_again = false;
    SetStatus(L"정리하는 중입니다… (큰 파일은 몇 초 걸립니다)");
    AfterTextChange();
    HANDLE th = CreateThread(nullptr, 8 << 20, ProcThread, j, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (th) CloseHandle(th);
    else {  // 스레드를 못 만들면 그냥 여기서
        ProcThread(j);
    }
}
// 뒤에서 한 정리가 끝남
static void OnProcDone(ProcJob* j) {
    g_busy = false;
    if (j->gen != g_gen || g_again) {  // 그 사이 글이나 옵션이 바뀜: 다시
        delete j;
        if (!g_text.empty()) StartJob(); else AfterTextChange();
        return;
    }
    MEAS_MARK(L"정리 엔진 (뒤에서 처리, 엔진만 " + meas::Num(j->ms) + L" ms)");
    FinishReprocess(*j);
    delete j;
    SetStatus((g_path.empty() ? L"정리했습니다." : L"정리했습니다: " + (g_mergeFiles.empty() ? g_path : FileName(g_path))) + g_restoreNote);
    g_restoreNote.clear();
    AfterTextChange();
}

static void Reprocess() {
    ++g_gen;
    if (g_text.empty()) {
        g_out.clear(); g_outToSrc.clear(); g_srcToOut.clear();
        SetRichText(g_dst, L"");
        g_dstDirty = false;
        g_metaSrc.clear(); g_metaDst.clear(); g_sizeText.clear(); g_chips.clear();
        return;
    }
    if (g_busy) { g_again = true; return; }
    if (g_text.size() > kAsyncChars) { MEAS_MARK(L"정리 시작 전"); StartJob(); return; }
    ProcJob j;
    j.o = ReadOptions();
    auto t0 = std::chrono::steady_clock::now();
    j.out = TidyParts(g_text, g_mergeBounds, j.o, &j.r, &j.map);
    j.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    MEAS_MARK(L"정리 엔진 (바로 처리)");
    FinishReprocess(j);
}

static bool ConfirmDiscardEdits() {
    if (!g_dstDirty) return true;
    return MessageBoxW(g_hwnd, L"결과 창에서 직접 고친 내용이 사라지고 새 설정으로 다시 정리됩니다.\n계속할까요?",
                       L"야문 클린리더", MB_OKCANCEL | MB_ICONQUESTION) == IDOK;
}

// 최근 파일 (ini [recent] r0..r9)
static std::vector<wstring> LoadRecent() {
    std::vector<wstring> v;
    for (int i = 0; i < 10; ++i) {
        wchar_t b[MAX_PATH * 2] = {};
        GetPrivateProfileStringW(L"recent", (L"r" + std::to_wstring(i)).c_str(), L"", b, MAX_PATH * 2, g_ini.c_str());
        if (b[0]) v.push_back(b);
    }
    return v;
}
static void SaveRecent(const std::vector<wstring>& v) {
    WritePrivateProfileStringW(L"recent", nullptr, nullptr, g_ini.c_str());
    for (size_t i = 0; i < v.size() && i < 10; ++i)
        WritePrivateProfileStringW(L"recent", (L"r" + std::to_wstring(i)).c_str(), v[i].c_str(), g_ini.c_str());
}
static void AddRecent(const wstring& path) {
    if (g_ini.empty() || path.empty()) return;
    std::vector<wstring> v = LoadRecent(), o;
    o.push_back(path);
    for (auto& x : v) if (lstrcmpiW(x.c_str(), path.c_str()) != 0) o.push_back(x);
    if (o.size() > 10) o.resize(10);
    SaveRecent(o);
}
static bool LoadFile(const wstring& path) {
    MEAS_BEGIN(wstring(L"파일 열기 — ") + (g_reading ? L"읽기 모드" : L"정리 모드"));
    wstring why = CheckTextFile(path, kMaxOpenBytes);
    if (!why.empty()) { ShowNotSupported(path, why); return false; }
    std::string bytes;
    MEAS_MARK(L"파일 검사");
    bool readOk = ReadAll(path, bytes);
    MEAS_MARK(L"파일 읽기 (" + SizeText((long long)bytes.size()) + L")");
    if (!readOk) {
#ifdef MEASURE
        meas::on = false;
#endif
        MessageBoxW(g_hwnd, (L"파일을 열 수 없습니다:\n" + path).c_str(), L"야문 클린리더", MB_ICONERROR);
        return false;
    }
    int enc = E_UTF8;
    U32 text;
    if (!ReadTextOf(path, bytes, text, enc, why)) { ShowNotSupported(path, why); return false; }
    MEAS_MARK(IsEpubPath(path) ? L"EPUB에서 본문 꺼내기" : L"인코딩 판별·글자 풀기");
    std::string().swap(bytes);
    g_text.swap(text);
    g_epub = IsEpubPath(path);
    g_enc = enc;
    g_path = path;
    g_mergeFiles.clear();
    g_mergeBounds.clear();
    g_lastSaved.clear();
    ShowSource();
    MEAS_MARK(L"원본 칸 준비 (읽기 모드는 비워 둠)");
    LoadWantPos(path);
    AddRecent(path);
    UpdateTitle();
    MEAS_MARK(L"읽던 곳·최근 파일 기록");
    Reprocess();
    if (!g_busy) { SetStatus(L"열었습니다: " + path + g_restoreNote); g_restoreNote.clear(); }
    return true;
}

// 보이지 않는 서식 문자가 들어 있는가 — ANSI(CP949)로 표현할 수 없으므로 UTF-8로 저장
static bool HasInvisible(const wstring& w) {
    for (wchar_t c : w) {
        unsigned u = (unsigned short)c;
        if (u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF || u == 0x2060 || u == 0xAD || u == 0x180E ||
            u == 0x200E || u == 0x200F || (u >= 0x202A && u <= 0x202E) || (u >= 0x2061 && u <= 0x2064) ||
            (u >= 0x2066 && u <= 0x2069) || (u >= 0xFE00 && u <= 0xFE0F))
            return true;
    }
    return false;
}

static bool SaveTo(const wstring& path) {
    if (g_busy) return false;
    wstring w = g_preview ? U32ToW(g_out, true) : GetRichText(g_dst);  // 큰 파일은 화면(앞부분)이 아니라 전체 결과
    int enc = SaveEncoding(g_enc);
    if (EncCp(enc) && HasInvisible(w)) enc = E_UTF8BOM;
    int lossy = 0;
    std::string bytes = Encode(w, enc, &lossy);
    if (lossy) {
        wstring m = wstring(kEncName[enc]) + L"로 표현할 수 없는 글자 " + std::to_wstring(lossy) +
                    L"개가 '?'로 바뀝니다.\n그래도 저장할까요? (아니요를 누르면 UTF-8로 저장합니다)";
        int r = MessageBoxW(g_hwnd, m.c_str(), L"야문 클린리더", MB_YESNOCANCEL | MB_ICONWARNING);
        if (r == IDCANCEL) return false;
        if (r == IDNO) bytes = Encode(w, E_UTF8BOM, nullptr), enc = E_UTF8BOM;
    }
    if (!WriteAll(path, bytes)) {
        MessageBoxW(g_hwnd, (L"저장하지 못했습니다:\n" + path).c_str(), L"야문 클린리더", MB_ICONERROR);
        return false;
    }
    {
        size_t sl = path.find_last_of(L"\\/");
        wstring dir = sl == wstring::npos ? L"" : path.substr(0, sl);
        SetStatus(L"저장했습니다: " + FileName(path) + L"   (" + wstring(kEncName[enc]) + L" · 폴더: " + dir + L")");
        g_statusOk = true;
        InvalidateRect(g_hwnd, &g_rcStatus, FALSE);
    }
    g_lastSaved = path;
    g_savedFlash = true;
    InvalidateRect(g_btnSave, nullptr, FALSE);
    SetTimer(g_hwnd, TIMER_SAVED, 2000, nullptr);
    return true;
}

static void DoSaveAs() {
    wchar_t buf[MAX_PATH * 4] = {};
    wstring def = g_path.empty() ? L"정리된 글.txt" : FileName(OutPath(g_path));
    wcsncpy(buf, def.c_str(), MAX_PATH * 4 - 1);
    OPENFILENAMEW of = {};
    of.lStructSize = sizeof(of);
    of.hwndOwner = g_hwnd;
    of.lpstrFilter = kFilter;
    of.lpstrFile = buf;
    of.nMaxFile = MAX_PATH * 4;
    of.lpstrDefExt = L"txt";
    of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (GetSaveFileNameW(&of)) SaveTo(buf);
}

static void DoSave() {
    if (g_text.empty() || g_busy) return;
    if (g_path.empty()) { DoSaveAs(); return; }
    wstring out = OutPath(g_path);
    if (FileExists(out) && _wcsicmp(out.c_str(), g_lastSaved.c_str()) != 0) {
        wstring m = L"이미 있는 파일입니다. 덮어쓸까요?\n\n" + out;
        if (MessageBoxW(g_hwnd, m.c_str(), L"야문 클린리더", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    SaveTo(out);
}

// 여러 파일을 순서대로 합쳐서 화면에 엶 (저장하면 "첫 파일_합본_정리.txt")
static wstring MergedPath(const std::vector<wstring>& files) {
    const wstring& f = files[0];
    size_t sl = f.find_last_of(L"\\/"), d = f.find_last_of(L'.');
    wstring stem = (d == wstring::npos || (sl != wstring::npos && d < sl)) ? f : f.substr(0, d);
    return stem + L"_합본.txt";
}
static bool LoadMerged(const std::vector<wstring>& files, HWND owner) {
    if (files.empty()) return false;
    U32 all;
    std::vector<size_t> bounds;
    std::vector<wstring> used;
    int firstEnc = -1;
    long long total = 0;
    wstring bad;
    for (auto& f : files) {
        wstring why = CheckTextFile(f, kMaxBatchBytes);
        if (!why.empty()) { bad += L"\n" + FileName(f) + L"  (" + why + L")"; continue; }
        std::string bytes;
        if (!ReadAll(f, bytes)) { bad += L"\n" + FileName(f) + L"  (읽을 수 없음)"; continue; }
        total += (long long)bytes.size();
        if (total > kMaxBatchBytes) {
            MessageBoxW(owner, (L"합친 크기가 " + std::to_wstring(kMaxBatchBytes / (1024 * 1024)) + L"MB를 넘어 합칠 수 없습니다.").c_str(),
                        L"여러 파일 합치기", MB_ICONWARNING);
            return false;
        }
        int enc = E_UTF8;
        U32 t;
        if (!ReadTextOf(f, bytes, t, enc, why)) { bad += L"\n" + FileName(f) + L"  (" + why + L")"; continue; }
        if (firstEnc < 0) firstEnc = enc;
        while (!t.empty() && t.back() == '\n') t.pop_back();
        if (!all.empty()) all += kMergeSep;  // 파일 사이는 빈 줄 두 개 (장면 구분처럼)
        bounds.push_back(all.size());
        used.push_back(f);
        all += t;
    }
    if (!bad.empty()) {
        wstring m = L"다음 파일은 지원하지 않거나 읽을 수 없어 빼고 합칩니다:" + bad;
        if (all.empty()) { MessageBoxW(owner, (L"합칠 수 있는 파일이 없습니다.\n" + bad).c_str(), L"여러 파일 합치기", MB_ICONWARNING); return false; }
        MessageBoxW(owner, m.c_str(), L"여러 파일 합치기", MB_ICONWARNING);
    }
    g_text.swap(all);
    g_enc = firstEnc < 0 ? E_UTF8 : firstEnc;
    g_mergeFiles = used;
    g_mergeBounds = bounds;
    g_epub = false;
    g_path = MergedPath(used);
    g_lastSaved.clear();
    ShowSource();
    UpdateTitle();
    Reprocess();
    if (!g_busy) SetStatus(std::to_wstring(used.size()) + L"개 파일을 순서대로 합쳤습니다. 저장하면 '" + FileName(OutPath(g_path)) + L"'로 저장됩니다.");
    return true;
}
// 읽기 인코딩을 바꿨을 때 다시 읽기
static void Reload() {
    if (!g_mergeFiles.empty()) { std::vector<wstring> f = g_mergeFiles; LoadMerged(f, g_hwnd); }
    else if (!g_path.empty()) LoadFile(g_path);
}

static void DoOpen() {
    wchar_t buf[MAX_PATH * 4] = {};
    OPENFILENAMEW of = {};
    of.lStructSize = sizeof(of);
    of.hwndOwner = g_hwnd;
    of.lpstrFilter = kOpenFilter;
    of.lpstrFile = buf;
    of.nMaxFile = MAX_PATH * 4;
    of.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&of)) LoadFile(buf);
}

// ---------------------------------------------------------------------------
// 디자인
// 화면 색 (밝게 / 어둡게 테마)
namespace col {
COLORREF win, panel, panelLine, label, border, text, sub, faint, groupTitle, checkMark, checkBorder, accent, accentHover,
    accentDown, hover, hoverBorder, chipBg, chipText, selBg, boxBorder, paneHead, status, segBg, segLine,
    field, fieldDis, comboHoverEdge, okText, segActive, segActiveEdge, segHover, segActiveText, swatchEdge, savedFill,
    accentDis, accentDisText, tbFill, tbHover, tbDown, tbEdge, tbEdgeHover, tbIcon, btnFill, btnDown, editText, editBg,
    body, toolSep, head;  // body: 글 칸 바깥 바탕, toolSep: 도구줄 묶음 사이 세로선, head: 메뉴 영역(도구줄·옵션·보기 설정) 바탕
}
static bool g_dark = false;
static HBRUSH g_brPanel, g_brField, g_brFieldDis;
static void ApplyThemeColors(bool dark) {
    using namespace col;
    g_dark = dark;
    if (!dark) {
        win = RGB(255, 255, 255); panel = RGB(247, 249, 252); panelLine = RGB(220, 226, 235); label = RGB(34, 39, 48);
        border = RGB(208, 214, 224); text = RGB(26, 30, 38); sub = RGB(60, 68, 82); faint = RGB(88, 96, 110);
        groupTitle = RGB(92, 112, 158); checkMark = RGB(50, 86, 158); checkBorder = RGB(96, 118, 160); accent = RGB(37, 99, 235);
        accentHover = RGB(29, 78, 216); accentDown = RGB(30, 64, 175); hover = RGB(241, 244, 249); hoverBorder = RGB(190, 199, 212);
        chipBg = RGB(228, 238, 255); chipText = RGB(29, 78, 216); selBg = RGB(232, 240, 254); boxBorder = RGB(150, 160, 176);
        paneHead = RGB(255, 255, 255); status = RGB(243, 245, 249); segBg = RGB(238, 241, 246); segLine = RGB(222, 227, 235);
        field = RGB(255, 255, 255); fieldDis = RGB(244, 246, 250); comboHoverEdge = RGB(150, 165, 190); okText = RGB(22, 128, 66);
        segActive = RGB(255, 255, 255); segActiveEdge = RGB(208, 214, 226); segHover = RGB(228, 233, 241); segActiveText = RGB(30, 64, 175);
        swatchEdge = RGB(150, 156, 168); savedFill = RGB(22, 140, 72); accentDis = RGB(214, 222, 238); accentDisText = RGB(96, 110, 140);
        tbFill = RGB(244, 246, 250); tbHover = RGB(234, 241, 253); tbDown = RGB(222, 230, 244); tbEdge = RGB(188, 196, 210);
        tbEdgeHover = RGB(140, 160, 200); tbIcon = RGB(44, 80, 158); btnFill = RGB(255, 255, 255); btnDown = RGB(230, 235, 242);
        editText = RGB(26, 30, 38); editBg = RGB(255, 255, 255);
        head = RGB(243, 245, 249); body = RGB(243, 245, 249); toolSep = RGB(220, 225, 233);  // 메뉴는 눈부시지 않은 옅은 회청색, 글 칸만 흰 종이
    } else {
        win = RGB(32, 33, 36); panel = RGB(39, 41, 45); panelLine = RGB(58, 61, 67); label = RGB(214, 218, 224);
        border = RGB(70, 74, 82); text = RGB(230, 232, 236); sub = RGB(170, 176, 186); faint = RGB(140, 146, 156);
        groupTitle = RGB(128, 156, 204); checkMark = RGB(138, 180, 248); checkBorder = RGB(120, 130, 150); accent = RGB(59, 130, 246);
        accentHover = RGB(37, 99, 235); accentDown = RGB(29, 78, 216); hover = RGB(48, 51, 56); hoverBorder = RGB(95, 100, 110);
        chipBg = RGB(37, 52, 82); chipText = RGB(147, 187, 252); selBg = RGB(45, 62, 95); boxBorder = RGB(110, 116, 126);
        paneHead = RGB(34, 36, 40); status = RGB(38, 40, 44); segBg = RGB(45, 48, 53); segLine = RGB(60, 64, 70);
        field = RGB(28, 29, 32); fieldDis = RGB(40, 42, 46); comboHoverEdge = RGB(110, 120, 140); okText = RGB(74, 200, 120);
        segActive = RGB(62, 66, 73); segActiveEdge = RGB(80, 85, 94); segHover = RGB(54, 57, 63); segActiveText = RGB(147, 187, 252);
        swatchEdge = RGB(120, 126, 136); savedFill = RGB(22, 140, 72); accentDis = RGB(52, 60, 78); accentDisText = RGB(130, 140, 160);
        tbFill = RGB(45, 48, 53); tbHover = RGB(54, 58, 66); tbDown = RGB(62, 67, 76); tbEdge = RGB(66, 70, 78);
        tbEdgeHover = RGB(100, 110, 130); tbIcon = RGB(147, 187, 252); btnFill = RGB(45, 48, 53); btnDown = RGB(58, 62, 70);
        editText = RGB(222, 224, 228); editBg = RGB(28, 29, 32);
        head = RGB(38, 40, 44); body = RGB(38, 40, 44); toolSep = RGB(62, 65, 72);
    }
    if (g_brPanel) DeleteObject(g_brPanel);
    if (g_brField) DeleteObject(g_brField);
    if (g_brFieldDis) DeleteObject(g_brFieldDis);
    g_brPanel = CreateSolidBrush(panel);
    g_brField = CreateSolidBrush(field);
    g_brFieldDis = CreateSolidBrush(fieldDis);
}
static bool SystemDark() {  // Windows: 앱 모드 어둡게?
    DWORD v = 1, sz = sizeof(v), type = 0;
    if (SHGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", &type, &v, &sz) == ERROR_SUCCESS)
        return v == 0;
    return false;
}
static void ApplyTitleBar(HWND h, bool accentEdge = false) {  // 제목 표시줄 색 (지원되는 Windows에서만)
    static HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm || !h) return;
    typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
    auto f = (SetAttr)(void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (!f) return;
    BOOL dk = g_dark;
    if (FAILED(f(h, 20, &dk, sizeof(dk)))) f(h, 19, &dk, sizeof(dk));
    COLORREF cap = accentEdge ? col::panel : (h == g_hwnd ? col::head : col::win), brd = accentEdge ? col::accent : col::border, tx = col::text;
    f(h, 35, &cap, sizeof(cap));
    f(h, 36, &tx, sizeof(tx));
    f(h, 34, &brd, sizeof(brd));
}
static void ApplyCtlTheme(HWND h, const wchar_t* darkName) {  // 선택목록 등도 어둡게 테마에 맞춤
    static HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux || !h) return;
    typedef HRESULT(WINAPI * SetTh)(HWND, LPCWSTR, LPCWSTR);
    if (auto f = (SetTh)(void*)GetProcAddress(ux, "SetWindowTheme")) f(h, g_dark ? darkName : nullptr, nullptr);
}
static void ApplyScrollTheme(HWND h) {  // 스크롤 막대도 테마에 맞춤
    static HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux || !h) return;
    typedef HRESULT(WINAPI * SetTh)(HWND, LPCWSTR, LPCWSTR);
    if (auto f = (SetTh)(void*)GetProcAddress(ux, "SetWindowTheme")) f(h, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}
using namespace Gdiplus;
static Color GC(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static void RoundPath(GraphicsPath& p, REAL x, REAL y, REAL w, REAL h, REAL r) {
    REAL d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    if (d < 1) { p.AddRectangle(RectF(x, y, w, h)); return; }
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}
static void FillRound(Graphics& g, const RECT& rc, int r, COLORREF fill, long long borderC = -1) {
    GraphicsPath path;
    REAL x = (REAL)rc.left, y = (REAL)rc.top, w = (REAL)(rc.right - rc.left), h = (REAL)(rc.bottom - rc.top);
    if (borderC >= 0) { x += 0.5f; y += 0.5f; w -= 1; h -= 1; }
    RoundPath(path, x, y, w, h, (REAL)r);
    SolidBrush b(GC(fill));
    g.FillPath(&b, &path);
    if (borderC >= 0) { Pen pen(GC((COLORREF)borderC), 1.0f); g.DrawPath(&pen, &path); }
}
static void FillRect2(HDC dc, const RECT& rc, COLORREF c) {
    SetBkColor(dc, c);
    ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
}
static void DrawTxt(HDC dc, const wstring& s, RECT rc, HFONT f, COLORREF c,
                    UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS) {
    HGDIOBJ old = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), (int)s.size(), &rc, fmt);
    SelectObject(dc, old);
}
static int MeasureW(HFONT f, const wstring& s) {
    HDC dc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(dc, f);
    SIZE sz = {};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    return sz.cx;
}
static int MeasureH(HFONT f) {
    HDC dc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(dc, f);
    TEXTMETRICW tm = {};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    return tm.tmHeight;
}
static wstring WinText(HWND h) {
    wchar_t buf[256];
    int n = GetWindowTextW(h, buf, 256);
    return wstring(buf, n);
}

// ---------------------------------------------------------------------------
// 사용자 규칙 파일
static const wchar_t* kRulesHelp =
    L"# 한 줄에 규칙 하나씩 적습니다. #으로 시작하는 줄은 설명입니다.\r\n"
    L"#\r\n"
    L"#   찾을 글 => 바꿀 글           찾은 글을 바꿉니다 (바꿀 글을 비우면 지웁니다)\r\n"
    L"#   줄삭제: 문구                 이 문구가 들어 있는 줄을 통째로 지웁니다 (광고 문구 등)\r\n"
    L"#   정규식: 패턴 => 바꿀 글      정규식으로 찾아 바꿉니다 (바꿀 글에 $1 $2 $& 사용)\r\n"
    L"#   줄삭제정규식: 패턴           정규식에 맞는 줄을 통째로 지웁니다\r\n"
    L"#\r\n"
    L"# 예)\r\n"
    L"#   줄삭제: 무단 배포\r\n"
    L"#   줄삭제: 텍본 제공\r\n"
    L"#   쟈 => 자\r\n"
    L"#   ㅡㅡ =>\r\n"
    L"#   정규식: ([!?])\\1+ => $1          (!!!! -> !, ???? -> ?)\r\n"
    L"#   정규식: ㅋ{3,} => ㅋㅋ              (ㅋㅋㅋㅋㅋ -> ㅋㅋ)\r\n"
    L"#   정규식: (\\d+)\\s*화 => 제$1화        (12 화 -> 제12화)\r\n"
    L"#   줄삭제정규식: ^\\s*-\\s*\\d+\\s*-\\s*$    (- 12 - 같은 쪽 번호 줄)\r\n"
    L"#\r\n"
    L"# 정규식은 한 문단(문장) 안에서 적용됩니다. 줄바꿈을 넣거나 빼는 규칙은 옵션이 대신합니다.\r\n";

// 규칙 한 줄 → 규칙 (kind: 0 바꾸기, 1 줄 지우기, 2 정규식 바꾸기, 3 정규식 줄 지우기)
struct RuleItem { int kind = 0; wstring f, t; bool on = true; };
static bool ParseRuleLine(const wstring& line, RuleItem& r) {
    auto trimL = [](wstring v) { size_t b = 0; while (b < v.size() && (v[b] == L' ' || v[b] == L'\t')) ++b; return v.substr(b); };
    auto trimR = [](wstring v) { while (!v.empty() && (v.back() == L' ' || v.back() == L'\t')) v.pop_back(); return v; };
    r = RuleItem();
    if (line.compare(0, 6, L"줄삭제정규식") == 0) {
        size_t c = line.find(L':');
        if (c == wstring::npos) return false;
        r.kind = 3; r.f = trimL(line.substr(c + 1));
        return !r.f.empty();
    }
    if (line.compare(0, 3, L"정규식") == 0) {
        size_t c = line.find(L':');
        if (c == wstring::npos) return false;
        wstring body = line.substr(c + 1);
        size_t arrow = body.find(L"=>");
        r.kind = 2;
        r.f = trimR(trimL(arrow == wstring::npos ? body : body.substr(0, arrow)));
        r.t = arrow == wstring::npos ? L"" : trimL(body.substr(arrow + 2));
        return !r.f.empty();
    }
    if (line.compare(0, 3, L"줄삭제") == 0) {
        size_t c = line.find(L':');
        if (c == wstring::npos) return false;
        wstring v = line.substr(c + 1);
        size_t b = 0; while (b < v.size() && v[b] == L' ') ++b;
        r.kind = 1; r.f = v.substr(b);
        return !r.f.empty();
    }
    size_t arrow = line.find(L"=>");
    if (arrow == wstring::npos) return false;
    wstring f = line.substr(0, arrow), t = line.substr(arrow + 2);
    while (!f.empty() && f.back() == L' ') f.pop_back();
    size_t b = 0; while (b < t.size() && t[b] == L' ') ++b;
    r.kind = 0; r.f = f; r.t = t.substr(b);
    return !r.f.empty();
}
static wstring RuleLine(const RuleItem& r);
// 규칙 파일에 적었다가 다시 읽어도 같은 규칙이 되는가 (앞뒤 공백·특수 머리말·=> 등)
static bool RuleStorable(const RuleItem& r) {
    if (r.f.empty() || r.f[0] == L'#') return false;
    RuleItem r2;
    wstring line = RuleLine(r);
    size_t a = 0;  // 파일을 읽을 때처럼 앞 공백을 떼고 읽어 봄
    while (a < line.size() && (line[a] == L' ' || line[a] == L'\t' || line[a] == 0xFEFF)) ++a;
    if (!ParseRuleLine(line.substr(a), r2)) return false;
    return r2.kind == r.kind && r2.f == r.f && r2.t == r.t;
}
static wstring RuleLine(const RuleItem& r) {
    wstring s = r.on ? L"" : L"#끔 ";
    switch (r.kind) {
    case 1: s += L"줄삭제: " + r.f; break;
    case 2: s += L"정규식: " + r.f + L" => " + r.t; break;
    case 3: s += L"줄삭제정규식: " + r.f; break;
    default: s += r.f + L" => " + r.t; break;
    }
    while (!s.empty() && s.back() == L' ') s.pop_back();  // 지우기 규칙: "찾을 말 =>"
    return s;
}
// 글 → 규칙 목록 (끈 규칙 "#끔 "도 읽음)
static std::vector<RuleItem> RulesFromText(const wstring& w, std::vector<wstring>* comments = nullptr) {
    std::vector<RuleItem> v;
    size_t i = 0;
    while (i <= w.size()) {
        size_t e = w.find(L'\n', i);
        if (e == wstring::npos) e = w.size();
        wstring line = w.substr(i, e - i);
        i = e + 1;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ' || line.back() == L'\t')) line.pop_back();
        size_t a = 0;
        while (a < line.size() && (line[a] == L' ' || line[a] == L'\t' || line[a] == 0xFEFF)) ++a;
        line = line.substr(a);
        bool on = true;
        if (line.compare(0, 3, L"#끔 ") == 0) {
            on = false;
            size_t k = 3; while (k < line.size() && (line[k] == L' ' || line[k] == L'\t')) ++k;
            line = line.substr(k);
        } else if (line.empty()) continue;
        else if (line[0] == L'#') {  // 설명 줄은 그대로 남김 (프로그램이 붙이는 머리줄은 빼고)
            if (comments && line.find(L"야문 클린리더 바꾸기·지우기 목록") == wstring::npos) comments->push_back(line);
            continue;
        }
        RuleItem r;
        if (ParseRuleLine(line, r)) { r.on = on; v.push_back(r); }
    }
    return v;
}
static void ParseRules(const wstring& w) {
    g_ruleReplace.clear();
    g_ruleDelete.clear();
    g_ruleRegex.clear();
    g_ruleRegexDel.clear();
    g_ruleErrors.clear();
    int lineNo = 0;
    auto valid = [&](const wstring& pat) {
        try { std::wregex re(pat, std::regex::ECMAScript); return true; }
        catch (...) { g_ruleErrors += L"\n" + std::to_wstring(lineNo) + L"번째 줄: " + pat; return false; }
    };
    size_t i = 0;
    while (i <= w.size()) {
        size_t e = w.find(L'\n', i);
        if (e == wstring::npos) e = w.size();
        wstring line = w.substr(i, e - i);
        i = e + 1;
        lineNo++;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ' || line.back() == L'\t')) line.pop_back();
        size_t a = 0;
        while (a < line.size() && (line[a] == L' ' || line[a] == L'\t' || line[a] == 0xFEFF)) ++a;
        line = line.substr(a);
        if (line.empty() || line[0] == L'#') continue;  // 설명·끈 규칙
        RuleItem r;
        if (!ParseRuleLine(line, r)) continue;
        switch (r.kind) {
        case 0: g_ruleReplace.push_back({WToU32(r.f.data(), r.f.size()), WToU32(r.t.data(), r.t.size())}); break;
        case 1: g_ruleDelete.push_back(WToU32(r.f.data(), r.f.size())); break;
        case 2: if (valid(r.f)) g_ruleRegex.push_back({r.f, r.t}); break;
        case 3: if (valid(r.f)) g_ruleRegexDel.push_back(r.f); break;
        }
    }
}
static wstring LoadRulesText() {
    std::string bytes;
    if (!ReadAll(g_rulesPath, bytes)) return wstring(kRulesHelp);
    int enc;
    U32 t = Decode(bytes, enc, 0);
    return U32ToW(NormalizeNL(t), true);
}

// ---------------------------------------------------------------------------
// 컨트롤
static bool g_fold = false;
struct PanelItem { RECT rc; wstring text; int kind; };  // kind 0 그룹 제목, 1 라벨, 2 흐린 설명
static std::vector<PanelItem> g_items;
static std::vector<PanelItem> g_readItems;  // 읽기 모드 보기 설정 줄의 이름표
static RECT g_rcLongBox;                    // 글자 수 칸 테두리
static RECT g_rcModeSeg;                    // 모드 전환 묶음
static bool g_showTitle = true;             // 도구줄에 프로그램 이름 표시
static std::vector<RECT> g_groupSep;  // 그룹 사이 세로선
static std::vector<int> g_toolSeps;    // 도구줄 묶음 사이 세로선 x
static int g_panelNeedW = 0, g_viewNeedW = 0;  // 옵션 칸 / 보기 설정 줄이 한 줄에 들어가는 창 안쪽 폭
static std::vector<RECT> g_viewSeps;  // 보기 설정 줄 묶음 사이 세로선
static int g_toolBtnLeft = 0;          // 도구줄 버튼들의 왼쪽 끝
static RECT g_rcTool, g_rcPanel, g_rcSrcHead, g_rcDstHead;
static std::vector<RECT> g_chipRc;
static std::vector<int> g_chipIdx;

static bool IsCheck(int id) {
    return id == ID_COLLAPSE || id == ID_JUNK || id == ID_SYNC || id == ID_HANJA || id == ID_HALF || id == ID_RP_CASE || id == ID_RP_WORD ||
           id == ID_RP_UP || id == ID_RP_REGEX || id == ID_RL_REGEX || id == ID_RL_LINE;
}

static LRESULT CALLBACK BtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!(GetWindowLongPtrW(h, GWLP_USERDATA) & 1)) {
            SetWindowLongPtrW(h, GWLP_USERDATA, GetWindowLongPtrW(h, GWLP_USERDATA) | 1);
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
            InvalidateRect(h, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        SetWindowLongPtrW(h, GWLP_USERDATA, GetWindowLongPtrW(h, GWLP_USERDATA) & ~1);
        InvalidateRect(h, nullptr, FALSE);
        break;
    case WM_LBUTTONDBLCLK:
        return DefSubclassProc(h, WM_LBUTTONDOWN, wp, lp);
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

static HWND Mk(const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0, HFONT f = nullptr) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, g_hwnd,
                             (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)(f ? f : g_uiFont), TRUE);
    return h;
}
static HWND MkBtn(int id, const wchar_t* text) {
    HWND h = Mk(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, id);
    SetWindowSubclass(h, BtnProc, 0, 0);
    return h;
}
static HWND MkCheck(int id, const wchar_t* text, bool on) {
    HWND h = MkBtn(id, text);
    SetToggle(h, on);
    return h;
}
// 배색 목록 항목: 그 배색의 바탕색 위에 이름을 글자색으로 (색과 이름이 함께 보여 따로 설명이 필요 없음)
static void DrawColorChip(HDC dc, RECT r, int idx, COLORREF edge) {
    COLORREF fg, bg;
    ViewColorsOf(idx, fg, bg);
    {
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillRound(g, r, S(3), bg, edge);
    }
    wchar_t buf[128];
    int n = (int)SendMessageW(g_cbVColor, CB_GETLBTEXT, idx, (LPARAM)buf);
    RECT tr = {r.left + S(8), r.top, r.right - S(4), r.bottom};
    DrawTxt(dc, wstring(buf, n > 0 ? n : 0), tr, g_uiFont, fg, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}
// 선택목록: 닫힌 모양을 직접 그림 (둥근 테두리 + 가는 화살표, 마우스를 올리면 테두리 강조)
static LRESULT CALLBACK ComboProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        if (w > 0 && hh > 0) {
            HDC dc = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, w, hh);
            HGDIOBJ ob = SelectObject(dc, bmp);
            FillRect2(dc, rc, ref ? col::panel : col::head);  // 모서리 바깥: 놓인 곳의 바탕색
            bool hover = (GetWindowLongPtrW(h, GWLP_USERDATA) & 1) != 0;
            bool open = SendMessageW(h, CB_GETDROPPEDSTATE, 0, 0) != 0;
            bool focus = GetFocus() == h;
            bool dis = !IsWindowEnabled(h);
            COLORREF edge = dis ? col::panelLine : (open || focus) ? col::accent : hover ? col::comboHoverEdge : col::border;
            COLORREF arrow = dis ? col::boxBorder : (open || hover) ? col::accent : col::faint;
            int sel = (int)SendMessageW(h, CB_GETCURSEL, 0, 0);
            // 배색 칸: 칸 전체를 그 배색으로 칠하고 이름을 그 글자색으로 (칸 안에 또 테두리 상자를 두지 않음)
            bool scheme = sel >= 0 && h == g_cbVColor && !dis;
            COLORREF sfg = 0, sbg = 0;
            if (scheme) {
                ViewColorsOf(sel, sfg, sbg);
                if (!(open || hover)) arrow = sfg;
            }
            {
                Graphics g(dc);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                FillRound(g, rc, S(5), scheme ? sbg : dis ? col::fieldDis : col::field, edge);
                REAL u = (REAL)g_dpi / 96.0f, cx = (REAL)w - 13.0f * u, cy = (REAL)hh / 2.0f;
                Pen pen(GC(arrow), 1.6f * u);
                pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
                PointF pts[3] = {PointF(cx - 3.5f * u, cy - 1.8f * u), PointF(cx, cy + 1.8f * u), PointF(cx + 3.5f * u, cy - 1.8f * u)};
                g.DrawLines(&pen, pts, 3);
            }
            if (scheme) {
                wchar_t buf[128];
                int n = (int)SendMessageW(h, CB_GETLBTEXT, sel, (LPARAM)buf);
                RECT tr = {S(9), 0, w - S(24), hh};
                DrawTxt(dc, wstring(buf, n > 0 ? n : 0), tr, g_uiFont, sfg, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            } else if (sel >= 0) {
                wchar_t buf[128];
                int n = (int)SendMessageW(h, CB_GETLBTEXT, sel, (LPARAM)buf);
                RECT tr = {S(9), 0, w - S(24), hh};
                DrawTxt(dc, wstring(buf, n > 0 ? n : 0), tr, g_uiFont, dis ? col::faint : col::text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }
            BitBlt(hdc, 0, 0, w, hh, dc, 0, 0, SRCCOPY);
            SelectObject(dc, ob);
            DeleteObject(bmp);
            DeleteDC(dc);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        if (!(GetWindowLongPtrW(h, GWLP_USERDATA) & 1)) {
            SetWindowLongPtrW(h, GWLP_USERDATA, GetWindowLongPtrW(h, GWLP_USERDATA) | 1);
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
            InvalidateRect(h, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        SetWindowLongPtrW(h, GWLP_USERDATA, GetWindowLongPtrW(h, GWLP_USERDATA) & ~1);
        InvalidateRect(h, nullptr, FALSE);
        break;
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case CB_SETCURSEL: {
        LRESULT r = DefSubclassProc(h, msg, wp, lp);
        InvalidateRect(h, nullptr, FALSE);
        return r;
    }
    }
    return DefSubclassProc(h, msg, wp, lp);
}
static HWND MkCombo(int id, std::initializer_list<const wchar_t*> items, int sel) {
    HWND h = Mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL, id);
    SetWindowSubclass(h, ComboProc, 0, 0);
    for (auto s : items) SendMessageW(h, CB_ADDSTRING, 0, (LPARAM)s);
    int cnt = (int)items.size();
    SendMessageW(h, CB_SETCURSEL, (sel >= 0 && sel < cnt) ? sel : 0, 0);
    return h;
}
// 글 칸에 파일을 놓으면 (읽기 모드처럼 고칠 수 없는 칸이어도) 열기와 똑같이 처리
// 칸이 새로 만들어질 수 있으므로 바로 열지 않고 메인 창에 넘김
static bool RpUndoAll();
static LRESULT CALLBACK DropProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_DROPFILES) {
        if (!PostMessageW(g_hwnd, WM_DROPFILES, wp, 0)) DragFinish((HDROP)wp);
        return 0;
    }
    if (msg == WM_COPY || msg == WM_CUT) { if (DmCopySel(h, msg == WM_CUT)) return 0; }
    if (msg == WM_PASTE && IsRichMode(h)) {  // 서식 텍스트 방식의 칸: 글자만 붙여 넣음
        UseBaseFontAtCaret(h);
        SendMessageW(h, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
        return 0;
    }
    if ((msg == WM_CHAR && wp >= 0x20) || msg == WM_IME_STARTCOMPOSITION) UseBaseFontAtCaret(h);
    if (msg == WM_LBUTTONDOWN && GetKeyState(VK_SHIFT) >= 0) {  // 선택한 글을 끌어 옮기지 않도록: 선택 위를 누르면 선택을 풀고 시작
        DispMap* m = DmOf(h);
        if (m && m->on) {
            CHARRANGE cr; SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&cr);
            if (cr.cpMax > cr.cpMin) {
                POINTL pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                long ci = (long)SendMessageW(h, EM_CHARFROMPOS, 0, (LPARAM)&pt);
                if (ci >= cr.cpMin && ci <= cr.cpMax) SendMessageW(h, EM_SETSEL, ci, ci);
            }
        }
    }
    if (msg == WM_KEYDOWN) {
        bool ctl = GetKeyState(VK_CONTROL) < 0, sh = GetKeyState(VK_SHIFT) < 0;
        bool copy = ctl && !sh && (wp == 'C' || wp == VK_INSERT), cut = (ctl && !sh && wp == 'X') || (sh && !ctl && wp == VK_DELETE);
        if ((copy || cut) && DmCopySel(h, cut)) return 0;
        if (((ctl && !sh && wp == 'V') || (sh && !ctl && wp == VK_INSERT)) && IsRichMode(h)) {  // 서식 텍스트 방식: 글자만 붙여 넣음
            UseBaseFontAtCaret(h);
            SendMessageW(h, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
            return 0;
        }
        if (ctl && !sh && wp == 'Z' && h == g_dst && RpUndoAll()) return 0;  // 바로 앞의 "모두 바꾸기" 되돌리기
    }
    return DefSubclassProc(h, msg, wp, lp);
}
static HWND MkRich(int id) {
    HWND h = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                             0, 0, 10, 10, g_hwnd, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessageW(h, EM_SETTEXTMODE, TM_PLAINTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE, 0);
    SendMessageW(h, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    LRESULT lo = SendMessageW(h, EM_GETLANGOPTIONS, 0, 0);
    SendMessageW(h, EM_SETLANGOPTIONS, 0, lo & ~(IMF_AUTOFONT | IMF_AUTOFONTSIZEADJUST | IMF_DUALFONT));
    SendMessageW(h, WM_SETFONT, (WPARAM)g_edFont, TRUE);
    SendMessageW(h, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_DROPFILES | ENM_SELCHANGE);
    // 고급 글자 배치(Uniscribe): 폭이 없는 서식 문자를 메모장처럼 폭 0으로 그림 (글자는 그대로)
    SendMessageW(h, EM_SETTYPOGRAPHYOPTIONS, TO_ADVANCEDTYPOGRAPHY, TO_ADVANCEDTYPOGRAPHY);
    RevokeDragDrop(h);  // 글 칸 자체의 끌어놓기 대신 파일 놓기(WM_DROPFILES)로 받음
    DragAcceptFiles(h, TRUE);
    SetWindowSubclass(h, DropProc, 1, 0);
    ApplyScrollTheme(h);
    return h;
}


// ---------------------------------------------------------------------------
// 읽던 위치 기억 (파일마다)
static wstring PosKey(const wstring& path) {
    unsigned long long h = 1469598103934665603ULL;
    for (wchar_t c : path) {
        wchar_t l = (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c;
        h ^= (unsigned)l; h *= 1099511628211ULL;
    }
    wchar_t buf[20]; swprintf(buf, 20, L"%016llx", h);
    return buf;
}
static void SaveReadPos() {
    if (g_posPath.empty() || g_ini.empty() || g_out.empty() || RichLen(g_dst) <= 0) return;
    long top = GlobalTop();
    if (g_pendingTop > top) top = g_pendingTop;  // 큰 글을 앞부분만 보는 중이면 기억해 둔 자리 유지
    if (top <= 0) {  // 맨 처음이면 기록을 지움
        WritePrivateProfileStringW(L"pos", PosKey(g_posPath).c_str(), nullptr, g_ini.c_str());
        return;
    }
    wstring v = std::to_wstring(top) + L"," + std::to_wstring((long)g_out.size()) + L"," + std::to_wstring((long)(time(nullptr) / 60));
    WritePrivateProfileStringW(L"pos", PosKey(g_posPath).c_str(), v.c_str(), g_ini.c_str());
    // 파일이 200개를 넘으면 오래된 것부터 지워 150개만 남김
    std::vector<wchar_t> buf(64 * 1024);
    DWORD got = GetPrivateProfileSectionW(L"pos", buf.data(), (DWORD)buf.size(), g_ini.c_str());
    if (got == 0 || got >= buf.size() - 2) return;
    struct E { wstring line; long stamp; };
    std::vector<E> es;
    for (const wchar_t* q = buf.data(); *q; q += wcslen(q) + 1) {
        long a = 0, b = 0, st = 0;
        const wchar_t* eq = wcschr(q, L'=');
        if (eq) swscanf(eq + 1, L"%ld,%ld,%ld", &a, &b, &st);
        es.push_back({q, st});
    }
    if (es.size() <= 200) return;
    std::sort(es.begin(), es.end(), [](const E& x, const E& y) { return x.stamp > y.stamp; });
    es.resize(150);
    wstring sec;
    for (auto& e : es) { sec += e.line; sec += L'\0'; }
    sec += L'\0';
    WritePrivateProfileSectionW(L"pos", sec.c_str(), g_ini.c_str());
}
static void LoadWantPos(const wstring& path) {
    g_wantIdx = -1; g_wantLen = 0;
    if (g_ini.empty()) return;
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"pos", PosKey(path).c_str(), L"", buf, 64, g_ini.c_str());
    long a = 0, b = 0;
    if (swscanf(buf, L"%ld,%ld", &a, &b) == 2 && a > 0 && b > 0) { g_wantIdx = a; g_wantLen = b; }
}

// 읽기 모드: 한 쪽 넘김 (화면 맨 아래 줄이 다음 쪽 첫 줄이 됨)
static void PageFlip(int dir) {
#ifdef MEASURE
    LARGE_INTEGER pf0; QueryPerformanceCounter(&pf0);
    struct FlipLog {
        LARGE_INTEGER a;
        ~FlipLog() {
            LARGE_INTEGER b; QueryPerformanceCounter(&b);
            LARGE_INTEGER f; QueryPerformanceFrequency(&f);
            long long ms = (b.QuadPart - a.QuadPart) * 1000 / f.QuadPart;
            if (meas::flipPending) {  // 열고 나서 첫 쪽 넘김
                meas::flipPending = false;
                meas::LogLine(L"[첫 쪽 넘김] " + meas::Num(ms) + L" ms" + (meas::mon ? L" (줄 나눔 지켜보는 중)" : L"") + L"\r\n\r\n");
                SetStatus(L"측정판: 첫 쪽 넘김 " + meas::Num(ms) + L" ms");
            }
        }
    } flipLog{pf0};
#endif
    RECT fr; SendMessageW(g_dst, EM_GETRECT, 0, (LPARAM)&fr);
    long first = (long)SendMessageW(g_dst, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (dir > 0) {  // 화면 맨 아래(잘린) 줄이 다음 쪽 맨 위로
        POINTL pt = {fr.left + 1, fr.bottom - 2};
        long ci = (long)SendMessageW(g_dst, EM_CHARFROMPOS, 0, (LPARAM)&pt);
        long last = (long)SendMessageW(g_dst, EM_EXLINEFROMCHAR, 0, ci);
        if (last <= first) last = first + 1;
        // 쪽 맨 위가 빈 줄이면 건너뜀. 다음 줄이 있는지는 그 줄만 물어봄
        // (전체 줄 수를 물으면 큰 글은 글 끝까지 줄 나눔이 끝날 때까지 기다리게 됨)
        for (int k = 0; k < 3; ++k) {
            long ci2 = (long)SendMessageW(g_dst, EM_LINEINDEX, last, 0);
            if (ci2 < 0 || SendMessageW(g_dst, EM_LINELENGTH, ci2, 0) > 0) break;
            if ((long)SendMessageW(g_dst, EM_LINEINDEX, last + 1, 0) < 0) break;  // 마지막 줄
            ++last;
        }
        long c = (long)SendMessageW(g_dst, EM_LINEINDEX, last, 0);
        if (c >= 0 && ci >= 0) ScrollToChar(g_dst, c);
    } else if (first > 0) {  // 지금 맨 위 줄이 앞쪽 화면의 맨 아래 줄이 되도록
        long fc = (long)SendMessageW(g_dst, EM_LINEINDEX, first, 0);
        POINTL a = {}, b = {};
        SendMessageW(g_dst, EM_POSFROMCHAR, (WPARAM)&a, fc);
        SendMessageW(g_dst, EM_POSFROMCHAR, (WPARAM)&b, (long)SendMessageW(g_dst, EM_LINEINDEX, first + 1, 0));
        int lh = b.y > a.y ? b.y - a.y : S(20);
        int room = (fr.bottom - fr.top) - lh;  // 새 맨 위 줄 ~ 지금 맨 위 줄 사이에 들어갈 높이
        long t = first - 1;
        for (long k = first - 1; k >= 0 && first - k < 400; --k) {
            POINTL p = {};
            SendMessageW(g_dst, EM_POSFROMCHAR, (WPARAM)&p, (long)SendMessageW(g_dst, EM_LINEINDEX, k, 0));
            if (a.y - p.y > room) break;
            t = k;
        }
        ScrollToChar(g_dst, (long)SendMessageW(g_dst, EM_LINEINDEX, t, 0));
    }
    {   // 커서도 보이는 첫 줄로 (화살표 키를 눌렀을 때 예전 자리로 튀지 않게)
        bool os = g_syncing; g_syncing = true;
        long c = TopChar(g_dst);
        SendMessageW(g_dst, EM_SETSEL, c, c);
        g_syncing = os;
    }
}
static int ReadPercent() {
    SCROLLINFO si = {sizeof(si), SIF_ALL};
    if (!GetScrollInfo(g_dst, SB_VERT, &si) || si.nMax <= si.nMin) return RichLen(g_dst) > 0 ? 100 : 0;
    double span = (double)si.nMax - si.nMin + 1;
    double seen = (double)si.nPos - si.nMin + std::max<UINT>(si.nPage, 1);
    double f = seen / span;
    if (f > 1) f = 1;
    int pct = (int)(f * 100.0 + 0.5);
    return pct > 100 ? 100 : pct < 0 ? 0 : pct;
}
static void SetReading(bool on);
// 결과 창: 읽기 모드에서 위쪽·아래쪽 클릭으로 쪽 넘김
static bool InFlipZone(HWND h, int y, int* dir) {
    RECT r; GetClientRect(h, &r);
    int hh = r.bottom - r.top;
    if (hh <= 0) return false;
    if (y < hh / 3) { *dir = -1; return true; }
    if (y > hh * 2 / 3) { *dir = 1; return true; }
    return false;
}
static void SetFullscreen(bool on);
static void StepFontSize(int dir);
static LRESULT CALLBACK DstProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (g_reading) {
        int dir = 0;
        switch (msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
            if (InFlipZone(h, (short)HIWORD(lp), &dir)) { SetFocus(h); PageFlip(dir); return 0; }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                if (InFlipZone(h, p.y, &dir)) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
            }
            break;
        case WM_KEYDOWN:
            if (wp == VK_SPACE) { PageFlip(GetKeyState(VK_SHIFT) < 0 ? -1 : 1); return 0; }
            if (wp == VK_NEXT) { PageFlip(1); return 0; }
            if (wp == VK_PRIOR) { PageFlip(-1); return 0; }
            if (wp == VK_ESCAPE) {  // 전체 화면 → 읽기 모드 순서로 닫음
                if (g_fullscreen) SetFullscreen(false);
                else SetReading(false);
                return 0;
            }
            break;
        case WM_CHAR:
            if (wp == L' ' || wp == 27) return 0;
            break;
        case WM_MOUSEWHEEL:  // Ctrl+휠: 글자 크기 (확대 대신)
            if (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL) { StepFontSize(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1); return 0; }
            break;
        }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

static void UpdateSwatches() {
    if (g_btnVFg) InvalidateRect(g_btnVFg, nullptr, FALSE);
    if (g_btnVBg) InvalidateRect(g_btnVBg, nullptr, FALSE);
}
// 보기 설정을 두 창에 적용
// 결과 창을 비우고 글꼴·모양을 정한 뒤 글을 다시 넣음 (보던 곳 유지)
// 긴 글에 글꼴·줄 간격을 바로 적용하면 글 전체를 여러 번 다시 배치해 느려서 이렇게 함
static void ReloadDst(long top) {
    bool dirty = g_dstDirty;
    bool fromOut = g_preview || !dirty;  // 직접 고친 적이 없으면 정리 결과에서 다시
    wstring keep;
    if (!fromOut) keep = GetRichText(g_dst);
    {
        bool ol = g_loading; g_loading = true;
        SETTEXTEX st = {ST_DEFAULT, 1200};
        if (RichLen(g_dst) > 20000) RecreateRich(g_dst);  // 긴 글은 새 칸으로
        else SendMessageW(g_dst, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)L"");
        SendMessageW(g_dst, WM_SETFONT, (WPARAM)(g_reading ? g_readFont : g_edFont), FALSE);
        g_loading = ol;
    }
    MEAS_MARK(L"결과 칸 비우기(새 칸)·글꼴 정하기");
    Layout();
    MEAS_MARK(L"창 배치(빈 칸)");
    if (fromOut) ShowResultText();
    else SetRichText(g_dst, keep);
    g_dstDirty = dirty;
    GoToGlobal(top);
    MEAS_MARK(top > 0 ? L"보던 곳으로 이동" : L"맨 처음으로 (이동 없음)");
    if (MEAS_ON()) {
        MEAS_NOTE(L"글자 수: 결과 " + meas::Num((long long)g_out.size()) + L"자" + (g_preview && !g_reading ? L" (정리 모드: 앞부분만 화면에)" : L" (화면에 전체)"));
        PostMessageW(g_hwnd, WM_APP_MEAS, 0, 0);
    }
}
// 보기 설정 적용: 글꼴·크기·줄 간격은 다시 넣기, 색·여백은 그대로 적용
static void ApplyView(bool fontChanged, bool relayout) {
    HFONT old = nullptr;
    if (fontChanged) { old = g_readFont; g_readFont = MakeReadFont(); }
    if (g_reading) {
        SetCursor(LoadCursor(nullptr, IDC_WAIT));
        if (fontChanged || relayout) { MEAS_BEGIN(L"보기 설정 바꾸기 (글꼴·크기·줄 간격 등)"); ReloadDst(GlobalTop()); }
        else { RecolorEdit(g_dst); Layout(); }
    }
    if (old) DeleteObject(old);  // 결과 창은 이미 새 글꼴을 씀
    UpdateSwatches();
    for (HWND e : {g_src, g_dst}) RedrawWindow(e, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}
static void SaveView() {
    WritePrivateProfileStringW(L"opt", L"vfont", g_vFont.c_str(), g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vsize", std::to_wstring(g_vSize).c_str(), g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vcolor2", std::to_wstring(g_vColor).c_str(), g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vfg", std::to_wstring((unsigned)g_vfg).c_str(), g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vbg", std::to_wstring((unsigned)g_vbg).c_str(), g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vline2", std::to_wstring(g_vLine).c_str(), g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vline", std::to_wstring(std::max(0, g_vLine - 1)).c_str(), g_ini.c_str());  // 예전 판이 읽는 값 (좁게는 보통으로)
    WritePrivateProfileStringW(L"opt", L"vbold", g_vBold ? L"1" : L"0", g_ini.c_str());
    WritePrivateProfileStringW(L"opt", L"vmargin", std::to_wstring(g_vMargin).c_str(), g_ini.c_str());
}
// 색 고르기 창 제목
static UINT_PTR CALLBACK ColorHook(HWND h, UINT msg, WPARAM, LPARAM lp) {
    if (msg == WM_INITDIALOG) { CHOOSECOLORW* cc = (CHOOSECOLORW*)lp; SetWindowTextW(h, (const wchar_t*)cc->lCustData); return TRUE; }
    return 0;
}
static bool PickColor(COLORREF& c, const wchar_t* title) {
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner = g_hwnd;
    cc.rgbResult = c;
    cc.lpCustColors = g_custCols;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
    cc.lpfnHook = ColorHook;
    cc.lCustData = (LPARAM)title;
    if (!ChooseColorW(&cc)) return false;
    c = cc.rgbResult;
    return true;
}
// 글자색 또는 바탕색 하나만 바꿈 → 색 조합은 "사용자 지정"
static void PickViewColor(bool text) {
    COLORREF fg, bg;
    ViewColors(fg, bg);
    COLORREF c = text ? fg : bg;
    if (!PickColor(c, text ? L"글자색 고르기" : L"바탕색 고르기")) return;
    g_vfg = text ? c : fg;
    g_vbg = text ? bg : c;
    g_vColor = kColorCustom;
    SendMessageW(g_cbVColor, CB_SETCURSEL, kColorCustom, 0);
    SaveView();
    ApplyView(false, false);
}

static void SetFullscreen(bool on);
static void SetReading(bool on) {
    if (on == g_reading) return;
    if (!on && g_fullscreen) SetFullscreen(false);
    MEAS_BEGIN(on ? L"모드 전환 — 정리 → 읽기" : L"모드 전환 — 읽기 → 정리");
    long top = GlobalTop();
    if (on && g_pendingTop > top) top = g_pendingTop;
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    g_reading = on;
    if (!on && g_srcStale) FillSource();
    MEAS_MARK(L"전환 준비");  // 읽기 모드에서 연 글: 이제 원본 칸을 채움
    SendMessageW(g_dst, EM_SETREADONLY, (on || g_preview) ? TRUE : FALSE, 0);
    ReloadDst(top);  // 글꼴·모양을 바꾸고 (큰 글은 알맞은 부분을) 다시 넣음
    g_pendingTop = (g_preview && !on && top >= RichLen(g_dst) - 1) ? top : -1;  // 정리 모드의 앞부분 밖에서 읽던 자리는 기억
    for (HWND b : {g_btnRead, g_btnTidy, g_btnSave, g_btnToc, g_btnFind, g_btnMark, g_btnFull}) InvalidateRect(b, nullptr, FALSE);  // 모드에 따라 모양이 다른 단추
    {   // 커서를 보이는 첫 줄로 (예전 자리에 남지 않게)
        bool os = g_syncing; g_syncing = true;
        long c = TopChar(g_dst);
        SendMessageW(g_dst, EM_SETSEL, c, c);
        g_syncing = os;
    }
    RedrawWindow(g_dst, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
    g_readPct = -1;
    if (on) { SetFocus(g_dst); SetTimer(g_hwnd, TIMER_READ, 300, nullptr); }
    else KillTimer(g_hwnd, TIMER_READ);
    IniSet(L"reading", on ? 1 : 0);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// 읽기 도구: 찾기 · 목차 · 책갈피 · 전체 화면 · 글자 크기
static bool IsInvisibleCh(char32_t u) {
    return u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF || u == 0x2060 || u == 0xAD || u == 0x180E ||
           u == 0x200E || u == 0x200F || (u >= 0x202A && u <= 0x202E) || (u >= 0x2061 && u <= 0x2064) ||
           (u >= 0x2066 && u <= 0x2069) || (u >= 0xFE00 && u <= 0xFE0F);
}
// 찾을 글: 보통은 정리 결과 전체, 정리 모드에서 직접 고친 글이면 지금 보이는 글
static U32 g_findHay;
static bool g_findOnOut = true;
static const U32& FindHay() {
    g_findOnOut = !g_dstDirty;
    if (g_findOnOut) return g_out;
    wstring w = GetRichText(g_dst);
    g_findHay = NormalizeNL(WToU32(w.data(), w.size()));
    return g_findHay;
}
// 글 위치(코드 포인트) → 결과 칸 글자 위치 (보조 평면 글자는 칸에서 2자리)
static long CpOf(const U32& hay, size_t idx) {
    long cp = (long)idx;
    for (size_t k = 0; k < idx && k < hay.size(); ++k) if (hay[k] >= 0x10000) cp++;
    return cp;
}

// 풍선 도움말: 직접 그린 둥근 말풍선 (부드러운 그림자, 이름 + 흐린 단축키·설명)
// 마우스를 단추에 잠시 올리면 단추 아래(자리가 없으면 위)에 뜨고, 누르거나 메뉴를 열면 사라짐
struct TipItem { HWND h; wstring name, key; };
static std::vector<TipItem> g_tips;
static HWND g_tipWnd = nullptr;    // 도움말 창 (계층 창)
static HWND g_tipHover = nullptr;  // 마우스가 올라간 단추
static HWND g_tipShown = nullptr;  // 지금 도움말이 떠 있는 단추
static HWND g_tipMuted = nullptr;  // 눌렀거나 메뉴를 연 단추: 마우스가 떠날 때까지 다시 띄우지 않음
static DWORD g_tipLastHide = 0;    // 방금까지 떠 있었으면 옆 단추로 옮길 때 바로 띄움
static const UINT_PTR kTipTimer = 1;
static TipItem* TipOf(HWND h) { for (auto& t : g_tips) if (t.h == h) return &t; return nullptr; }
static void TipWindowHide() {
    if (g_tipWnd && IsWindowVisible(g_tipWnd)) { ShowWindow(g_tipWnd, SW_HIDE); g_tipLastHide = GetTickCount(); }
    g_tipShown = nullptr;
}
static void HideTip() {  // 팝업 메뉴·펼친 목록을 열 때 등
    if (g_tipWnd) KillTimer(g_tipWnd, kTipTimer);
    TipWindowHide();
    if (g_tipHover) g_tipMuted = g_tipHover;
}
static void ShowTipNow(HWND b) {
    TipItem* t = TipOf(b);
    if (!t || !g_tipWnd || !IsWindowVisible(b)) return;
    HDC sdc = GetDC(nullptr);
    Gdiplus::Font font(sdc, g_smallFont);
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);
    RectF rn, rk;
    {
        Graphics mg(sdc);
        mg.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        mg.MeasureString(t->name.c_str(), -1, &font, PointF(0, 0), &sf, &rn);
        if (!t->key.empty()) mg.MeasureString(t->key.c_str(), -1, &font, PointF(0, 0), &sf, &rk);
    }
    const int padX = S(8), padY = S(4), gap = S(8), sh = S(4), rad = S(4);
    int wName = (int)ceil(rn.Width) + 1, wKey = t->key.empty() ? 0 : (int)ceil(rk.Width) + 1;
    int th = std::max((int)ceil(rn.Height), MeasureH(g_smallFont));
    int bw = padX * 2 + wName + (wKey ? gap + wKey : 0), bh = padY * 2 + th;
    int W = bw + sh * 2, H = bh + sh * 2;
    RECT br; GetWindowRect(b, &br);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&br, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;
    int x = (br.left + br.right) / 2 - bw / 2, y = br.bottom + S(4);
    if (y + bh > wa.bottom) y = br.top - S(4) - bh;  // 아래에 자리가 없으면 위로
    if (x + bw > wa.right - S(4)) x = wa.right - S(4) - bw;
    if (x < wa.left + S(4)) x = wa.left + S(4);
    // 메뉴보다 눈에 띄지 않게: 밝은 테마는 흰 바탕 + 옅은 테두리, 어두운 테마는 메뉴보다 조금 밝은 회색
    COLORREF bg = g_dark ? RGB(46, 50, 56) : RGB(255, 255, 255), edge = g_dark ? RGB(72, 78, 88) : RGB(214, 218, 225);
    COLORREF tc = g_dark ? RGB(222, 226, 231) : RGB(52, 58, 66), kc = g_dark ? RGB(146, 154, 166) : RGB(140, 147, 158);
    Bitmap bmp(W, H, PixelFormat32bppPARGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(Color(0, 0, 0, 0));
        for (int k = sh; k >= 1; --k) {  // 부드러운 그림자 (아래로 조금 치우침)
            GraphicsPath p;
            RoundPath(p, (REAL)(sh - k), (REAL)(sh - k + S(1)), (REAL)(bw + 2 * k), (REAL)(bh + 2 * k), (REAL)(rad + k));
            SolidBrush sb(Color((BYTE)(2 + 5 * (sh - k + 1) / sh), 0, 0, 0));
            g.FillPath(&sb, &p);
        }
        GraphicsPath body;
        RoundPath(body, (REAL)sh, (REAL)sh, (REAL)bw, (REAL)bh, (REAL)rad);
        SolidBrush bb(GC(bg));
        g.FillPath(&bb, &body);
        if (edge != bg) { Pen ep(GC(edge), 1.0f); g.DrawPath(&ep, &body); }
        SolidBrush tb(GC(tc)), kb(GC(kc));
        REAL ty = (REAL)sh + padY + (th - rn.Height) / 2;
        g.DrawString(t->name.c_str(), -1, &font, PointF((REAL)(sh + padX), ty), &sf, &tb);
        if (wKey) g.DrawString(t->key.c_str(), -1, &font, PointF((REAL)(sh + padX + wName + gap), ty), &sf, &kb);
    }
    // 계층 창에 그대로 올림 (모서리·그림자는 투명도로)
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        BitmapData bd;
        Rect lr(0, 0, W, H);
        if (bmp.LockBits(&lr, ImageLockModeRead, PixelFormat32bppPARGB, &bd) == Ok) {
            for (int row = 0; row < H; ++row) memcpy((BYTE*)bits + (size_t)row * W * 4, (BYTE*)bd.Scan0 + (size_t)row * bd.Stride, (size_t)W * 4);
            bmp.UnlockBits(&bd);
            HDC mdc = CreateCompatibleDC(sdc);
            HGDIOBJ ob = SelectObject(mdc, dib);
            POINT pos = {x - sh, y - sh}, src = {0, 0};
            SIZE sz = {W, H};
            BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            UpdateLayeredWindow(g_tipWnd, sdc, &pos, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
            SelectObject(mdc, ob);
            DeleteDC(mdc);
            SetWindowPos(g_tipWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            g_tipShown = b;
        }
    }
    if (dib) DeleteObject(dib);
    ReleaseDC(nullptr, sdc);
}
static LRESULT CALLBACK TipWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_TIMER:
        if (wp == kTipTimer) {
            KillTimer(h, kTipTimer);
            HWND b = g_tipHover;
            if (!b || b == g_tipMuted) return 0;
            POINT pt; GetCursorPos(&pt);
            RECT r; GetWindowRect(b, &r);
            HWND fg = GetForegroundWindow();
            bool ours = fg == g_hwnd || (fg && GetWindow(fg, GW_OWNER) == g_hwnd);
            if (PtInRect(&r, pt) && ours) ShowTipNow(b);
        }
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static LRESULT CALLBACK TipHoverProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_MOUSEMOVE:
        if (g_tipHover != h) {
            if (g_tipShown && g_tipShown != h) TipWindowHide();
            if (g_tipMuted != h) g_tipMuted = nullptr;
            g_tipHover = h;
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
            if (g_tipWnd && g_tipMuted != h) {
                bool quick = g_tipShown || GetTickCount() - g_tipLastHide < 600;  // 옆 단추로 옮겨 가면 바로
                SetTimer(g_tipWnd, kTipTimer, quick ? 60 : 450, nullptr);
            }
        }
        break;
    case WM_MOUSELEAVE:
        if (g_tipHover == h) {
            if (g_tipWnd) KillTimer(g_tipWnd, kTipTimer);
            TipWindowHide();
            g_tipHover = nullptr;
            if (g_tipMuted == h) g_tipMuted = nullptr;
        }
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MOUSEWHEEL: case WM_KEYDOWN:
        if (g_tipHover == h || g_tipShown == h) HideTip();
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(h, TipHoverProc, 77);
        if (g_tipHover == h) g_tipHover = nullptr;
        if (g_tipShown == h) TipWindowHide();
        break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
// name: 이름, key: 흐리게 덧붙일 단축키·설명 (없으면 nullptr)
static void AddTip(HWND b, const wchar_t* name, const wchar_t* key = nullptr) {
    if (!b) return;
    if (TipItem* t = TipOf(b)) { t->name = name; t->key = key ? key : L""; return; }
    g_tips.push_back({b, name, key ? key : L""});
    SetWindowSubclass(b, TipHoverProc, 77, 0);
}
static void SetTip(HWND b, const wchar_t* name, const wchar_t* key = nullptr) {
    TipItem* t = TipOf(b);
    if (!t) return;
    t->name = name; t->key = key ? key : L"";
    if (g_tipShown == b) ShowTipNow(b);
}
static void CreateTipWindow() {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = TipWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"YamoonTip";
    RegisterClassExW(&wc);
    g_tipWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, L"YamoonTip", L"",
                               WS_POPUP, 0, 0, 0, 0, g_hwnd, nullptr, g_hInst, nullptr);
}
static void FullTip() {  // 전체 화면 단추 (켜짐에 따라 이름이 바뀜)
    if (g_fullscreen) SetTip(g_btnFull, L"전체 화면 끝", L"F11 · Esc");
    else SetTip(g_btnFull, L"전체 화면", L"F11");
}

// 목차: 회차·장 제목 줄
static bool IsDigitCh(char32_t c) { return (c >= '0' && c <= '9') || (c >= 0xFF10 && c <= 0xFF19); }
static bool StartsWithStr(const U32& t, size_t i, const char32_t* w) {
    for (; *w; ++w, ++i) if (i >= t.size() || t[i] != *w) return false;
    return true;
}
static bool IsChapterLine(const U32& t, size_t a, size_t b) {
    while (a < b && (t[a] == ' ' || t[a] == 0x3000 || t[a] == '\t' || IsInvisibleCh(t[a]))) ++a;
    while (b > a && (t[b - 1] == ' ' || t[b - 1] == 0x3000 || t[b - 1] == '\t' || IsInvisibleCh(t[b - 1]))) --b;
    if (b <= a || b - a > 40) return false;
    size_t i = a;
    if (t[i] == '[' || t[i] == 0x3010 || t[i] == '<' || t[i] == 0x3008 || t[i] == 0x300A || t[i] == 0x300C || t[i] == 0x300E || t[i] == '#') ++i;
    while (i < b && t[i] == ' ') ++i;
    for (const char32_t* w : {U"프롤로그", U"에필로그", U"외전", U"서장", U"종장", U"Prologue", U"Epilogue", U"PROLOGUE", U"EPILOGUE"})
        if (StartsWithStr(t, i, w)) return true;
    size_t j = i;
    if (StartsWithStr(t, j, U"Chapter") || StartsWithStr(t, j, U"CHAPTER")) { j += 7; while (j < b && t[j] == ' ') ++j; return j < b && IsDigitCh(t[j]); }
    bool je = t[j] == U'제' || t[j] == U'第';
    if (je) { ++j; while (j < b && t[j] == ' ') ++j; }
    size_t d = j;
    while (j < b && IsDigitCh(t[j])) ++j;
    if (j == d) return false;
    while (j < b && t[j] == ' ') ++j;
    if (j >= b) return false;
    char32_t u = t[j];
    if (u == U'화' || u == U'話' || u == U'回' || u == U'章') return true;
    if (je && (u == U'장' || u == U'권' || u == U'부' || u == U'회' || u == U'편' || u == U'막')) return true;
    if (!je && u == U'장' && (j + 1 >= b || t[j + 1] == ' ' || t[j + 1] == '.' || t[j + 1] == ']' || t[j + 1] == 0x3011)) return true;
    return false;
}
static void ShowTocMenu(HWND anchor) {
    HideTip();
    const U32& hay = FindHay();
    std::vector<size_t> pos;
    size_t a = 0;
    for (size_t i = 0; i <= hay.size() && pos.size() < 3000; ++i) {
        if (i == hay.size() || hay[i] == '\n') { if (i > a && IsChapterLine(hay, a, i)) pos.push_back(a); a = i + 1; }
    }
    HMENU m = CreatePopupMenu();
    if (pos.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"회차·장 제목을 찾지 못했습니다");
    long top = g_findOnOut ? GlobalTop() : TopChar(g_dst);
    int cur = -1;
    for (size_t k = 0; k < pos.size(); ++k) if ((long)pos[k] <= top + 2) cur = (int)k;
    for (size_t k = 0; k < pos.size(); ++k) {
        size_t e = pos[k];
        while (e < hay.size() && hay[e] != '\n') ++e;
        U32 line;
        for (size_t q = pos[k]; q < e && line.size() < 40; ++q) if (!IsInvisibleCh(hay[q])) line += hay[q];
        wstring w = U32ToW(line, false);
        for (auto& ch : w) if (ch == L'&') ch = L'+';
        AppendMenuW(m, MF_STRING | ((int)k == cur ? MF_CHECKED : 0) | (k && k % 40 == 0 ? MF_MENUBARBREAK : 0), ID_TOC_BASE + (UINT)k, w.c_str());
    }
    RECT r; GetWindowRect(anchor, &r);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd >= ID_TOC_BASE && cmd < ID_TOC_BASE + (int)pos.size()) {
        size_t at = pos[cmd - ID_TOC_BASE];
        ScrollToChar(g_dst, CpOf(hay, at));
        g_readPct = -1;
    }
    SetFocus(g_dst);
}

// 책갈피 (파일마다): [mark] 파일키 = 글자수;위치,위치,...
static std::vector<long> LoadMarks() {
    std::vector<long> v;
    if (g_posPath.empty() || g_ini.empty()) return v;
    std::vector<wchar_t> buf(8192);
    GetPrivateProfileStringW(L"mark", PosKey(g_posPath).c_str(), L"", buf.data(), (DWORD)buf.size(), g_ini.c_str());
    long len = 0;
    const wchar_t* p = buf.data();
    if (swscanf(p, L"%ld", &len) != 1) return v;
    p = wcschr(p, L';');
    while (p && *p) {
        ++p;
        long x = 0;
        if (swscanf(p, L"%ld", &x) == 1) v.push_back((len > 0 && len != (long)g_out.size()) ? (long)((double)x * g_out.size() / len) : x);
        p = wcschr(p, L',');
    }
    std::sort(v.begin(), v.end());
    return v;
}
static void SaveMarks(const std::vector<long>& v) {
    if (g_posPath.empty() || g_ini.empty()) return;
    if (v.empty()) { WritePrivateProfileStringW(L"mark", PosKey(g_posPath).c_str(), nullptr, g_ini.c_str()); return; }
    wstring s = std::to_wstring((long)g_out.size());
    for (size_t i = 0; i < v.size(); ++i) s += (i ? L"," : L";") + std::to_wstring(v[i]);
    WritePrivateProfileStringW(L"mark", PosKey(g_posPath).c_str(), s.c_str(), g_ini.c_str());
}
static void AddMark() {
    if (!g_reading) return;
    if (g_posPath.empty()) { SetStatus(L"책갈피는 파일로 연 글에서 쓸 수 있습니다."); return; }
    std::vector<long> v = LoadMarks();
    long top = GlobalTop();
    for (long x : v) if (labs(x - top) < 20) { SetStatus(L"이미 책갈피가 있는 곳입니다."); return; }
    if (v.size() >= 100) v.erase(v.begin());
    v.push_back(top);
    std::sort(v.begin(), v.end());
    SaveMarks(v);
    SetStatus(L"책갈피를 추가했습니다. (" + std::to_wstring(g_out.empty() ? 0 : (int)(100.0 * top / g_out.size() + 0.5)) + L"%)");
}
static void ShowMarkMenu(HWND anchor) {
    HideTip();
    std::vector<long> v = LoadMarks();
    HMENU m = CreatePopupMenu();
    bool file = !g_posPath.empty();
    AppendMenuW(m, MF_STRING | (file ? 0 : MF_GRAYED), ID_MARK_ADD, L"지금 위치에 책갈피 추가\tCtrl+B");
    if (!file) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"(파일로 연 글에서 쓸 수 있습니다)");
    if (!v.empty()) AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    for (size_t k = 0; k < v.size(); ++k) {
        size_t a = (size_t)std::max(0L, v[k]);
        if (a > g_out.size()) a = g_out.size();
        while (a < g_out.size() && (g_out[a] == '\n' || g_out[a] == ' ')) ++a;
        U32 line;
        for (size_t q = a; q < g_out.size() && g_out[q] != '\n' && line.size() < 30; ++q) if (!IsInvisibleCh(g_out[q])) line += g_out[q];
        int pct = g_out.empty() ? 0 : (int)(100.0 * v[k] / g_out.size() + 0.5);
        wstring w = std::to_wstring(pct) + L"%   " + U32ToW(line, false);
        for (auto& ch : w) if (ch == L'&') ch = L'+';
        AppendMenuW(m, MF_STRING, ID_MARK_BASE + (UINT)k, w.c_str());
    }
    if (!v.empty()) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, ID_MARK_CLEAR, L"이 글의 책갈피 모두 지우기"); }
    RECT r; GetWindowRect(anchor, &r);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd == ID_MARK_ADD) AddMark();
    else if (cmd == ID_MARK_CLEAR) { SaveMarks({}); SetStatus(L"이 글의 책갈피를 모두 지웠습니다."); }
    else if (cmd >= ID_MARK_BASE && cmd < ID_MARK_BASE + (int)v.size()) { GoToGlobal(v[cmd - ID_MARK_BASE]); g_readPct = -1; }
    SetFocus(g_dst);
}

// 화면 테마: 0 Windows 설정 따르기, 1 밝게, 2 어둡게
static void RethemeSubWindows();
static void SetTheme(int mode) {
    g_themeMode = (mode >= 0 && mode <= 2) ? mode : 0;
    ApplyThemeColors(g_themeMode == 0 ? SystemDark() : g_themeMode == 2);
    ApplyTitleBar(g_hwnd);
    for (HWND e : {g_src, g_dst}) { ApplyScrollTheme(e); RecolorEdit(e); }
    RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    RethemeSubWindows();
}
static void ShowThemeMenu(HWND anchor) {
    HideTip();
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_themeMode == 0 ? MF_CHECKED : 0), ID_THEME_SYS, L"Windows 설정 따르기");
    AppendMenuW(m, MF_STRING | (g_themeMode == 1 ? MF_CHECKED : 0), ID_THEME_LIGHT, L"밝게");
    AppendMenuW(m, MF_STRING | (g_themeMode == 2 ? MF_CHECKED : 0), ID_THEME_DARK, L"어둡게");
    RECT r; GetWindowRect(anchor, &r);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, r.right, r.bottom, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd >= ID_THEME_SYS && cmd <= ID_THEME_DARK) {
        SetTheme(cmd - ID_THEME_SYS);
        IniSet(L"theme", g_themeMode);
        SetStatus(wstring(L"화면 테마: ") + (g_themeMode == 0 ? L"Windows 설정 따르기" : g_themeMode == 1 ? L"밝게" : L"어둡게"));
    }
}
static void ShowRecentMenu(HWND anchor) {
    HideTip();
    std::vector<wstring> v = LoadRecent();
    HMENU m = CreatePopupMenu();
    if (v.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"(최근에 연 파일이 없습니다)");
    for (size_t k = 0; k < v.size(); ++k) {
        size_t sl = v[k].find_last_of(L"\\/");
        wstring name = sl == wstring::npos ? v[k] : v[k].substr(sl + 1);
        for (auto& ch : name) if (ch == L'&') ch = L'+';
        wchar_t buf[64] = {};
        GetPrivateProfileStringW(L"pos", PosKey(v[k]).c_str(), L"", buf, 64, g_ini.c_str());
        long a = 0, b = 0;
        wstring w = std::to_wstring(k + 1 == 10 ? 0 : k + 1) + L"  " + name;
        if (swscanf(buf, L"%ld,%ld", &a, &b) == 2 && b > 0) w += L"\t" + std::to_wstring((int)(100.0 * a / b + 0.5)) + L"%";
        AppendMenuW(m, MF_STRING | (GetFileAttributesW(v[k].c_str()) == INVALID_FILE_ATTRIBUTES ? MF_GRAYED : 0), ID_RECENT_BASE + (UINT)k, w.c_str());
    }
    if (!v.empty()) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, ID_RECENT_CLEAR, L"목록 비우기"); }
    RECT r; GetWindowRect(anchor, &r);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd == ID_RECENT_CLEAR) { SaveRecent({}); SetStatus(L"최근 파일 목록을 비웠습니다."); }
    else if (cmd >= ID_RECENT_BASE && cmd < ID_RECENT_BASE + (int)v.size()) { wstring path = v[cmd - ID_RECENT_BASE]; LoadFile(path); }
}

// 전체 화면 (읽기 모드)
static WINDOWPLACEMENT g_wpBeforeFull = {sizeof(WINDOWPLACEMENT)};
static LONG g_styleBeforeFull = 0;
static void SetFullscreen(bool on) {
    if (on == g_fullscreen) return;
    if (on && !g_reading) SetReading(true);
    if (on) {
        g_wpBeforeFull.length = sizeof(g_wpBeforeFull);
        GetWindowPlacement(g_hwnd, &g_wpBeforeFull);
        g_styleBeforeFull = GetWindowLongW(g_hwnd, GWL_STYLE);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        g_fullscreen = true;
        SetWindowLongW(g_hwnd, GWL_STYLE, (g_styleBeforeFull & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    } else {
        g_fullscreen = false;
        SetWindowLongW(g_hwnd, GWL_STYLE, g_styleBeforeFull);
        SetWindowPlacement(g_hwnd, &g_wpBeforeFull);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    SetWindowTextW(g_btnFull, g_fullscreen ? L"전체 화면 끝" : L"전체 화면");
    FullTip();
    Layout();
    InvalidateRect(g_hwnd, nullptr, TRUE);
    for (HWND b : {g_btnToc, g_btnFind, g_btnMark, g_btnFull}) InvalidateRect(b, nullptr, FALSE);  // 놓인 곳에 따라 바탕색이 다름
    SetFocus(g_dst);
}
// 글자 크기 한 단계 (Ctrl+휠, Ctrl + / -)
static void StepFontSize(int dir) {
    int n = (int)(sizeof(kSizes) / sizeof(*kSizes)), i = 0;
    for (int k = 0; k < n; ++k) if (kSizes[k] <= g_vSize) i = k;
    i += dir;
    if (i < 0 || i >= n) return;
    g_vSize = kSizes[i];
    SendMessageW(g_cbVSize, CB_SETCURSEL, i, 0);
    SaveView();
    ApplyView(true, true);
}

static void CreateUI() {
    g_btnAbout = MkBtn(ID_ABOUT, L"정보");
    g_btnFold = MkBtn(ID_FOLD, L"옵션 접기");
    g_btnReset = MkBtn(ID_RESET, L"옵션 초기화");
    g_btnOpen = MkBtn(ID_OPEN, L"열기");
    g_btnRecent = MkBtn(ID_RECENT, L"");
    g_btnTheme = MkBtn(ID_THEME, L"");
    g_btnBatch = MkBtn(ID_BATCH, L"여러 파일 변환");
    g_btnSaveAs = MkBtn(ID_SAVEAS, L"다른 이름으로 저장");
    g_btnSave = MkBtn(ID_SAVE, L"저장");

    g_cbBlank = MkCombo(ID_BLANK, {L"자동 판단", L"빈 줄 = 문단 구분", L"빈 줄 1개는 무시"}, IniGet(L"blank", 0));
    g_cbJoin = MkCombo(ID_JOIN, {L"자동으로 합치기", L"합치지 않음"}, IniGet(L"join", 0));
    g_cbSpace = MkCombo(ID_SPACE, {L"자동", L"항상 띄움", L"항상 붙임"}, IniGet(L"space", 0));

    g_cbSplit = MkCombo(ID_SPLIT, {L"문장마다 줄바꿈", L"긴 문단만 나누기", L"나누지 않음"},
                         SplitMap(IniGet(L"optver", 1) >= 2 ? IniGet(L"split", 1) : 1));  // ini에는 엔진 값 저장. 옵션판 2부터 기본: 문장마다 줄바꿈
    g_edLong = Mk(L"EDIT", std::to_wstring(IniGet(L"longlen", 400)).c_str(), ES_NUMBER | ES_CENTER | WS_TABSTOP, ID_LONGLEN);
    EnableWindow(g_edLong, SplitMode() == 0);  // 글자 수는 "긴 문단만 나누기"일 때만 쓰임
    g_cbParaSep = MkCombo(ID_PARASEP, {L"빈 줄 넣기", L"줄바꿈만"}, IniGet(L"parasep", 0));
    g_cbScene = MkCombo(ID_SCENE, {L"빈 줄 하나 더", L"* * * 넣기", L"구분 안 함"}, IniGet(L"scene", 0));

    g_cbIndent = MkCombo(ID_INDENT, {L"없음", L"공백 1칸", L"공백 2칸", L"전각 공백"}, IniGet(L"indent", 0));
    g_cbQuote = MkCombo(ID_QUOTE, {L"그대로", L"둥근 “ ” ‘ ’", L"곧은 \" '"}, IniGet(L"quote", 0));
    g_cbEll = MkCombo(ID_ELLIPSIS, {L"그대로", L"…", L"……", L"..."}, IniGet(L"ellipsis", 0));

    g_chkCollapse = MkCheck(ID_COLLAPSE, L"공백·문장부호 다듬기", IniGet(L"collapse", 1) != 0);
    g_chkJunk = MkCheck(ID_JUNK, L"쪽 번호·광고·HTML 제거", IniGet(L"junk", 0) != 0);
    g_chkHanja = MkCheck(ID_HANJA, L"괄호 한자 삭제", IniGet(L"hanja", 0) != 0);
    g_chkHalf = MkCheck(ID_HALF, L"전각→반각", IniGet(L"half", 0) != 0);

    g_cbReadEnc = MkCombo(ID_READENC, {L"자동 판별", L"UTF-8", L"ANSI (CP949)", L"UTF-16", L"일본어 (Shift-JIS)", L"중국어 간체 (GBK)",
                                       L"중국어 번체 (Big5)", L"서유럽 (Windows-1252)", L"한글 조합형 (Johab)"}, IniGet(L"readenc", 0));
    g_forceEnc = CbSel(g_cbReadEnc);
    g_cbEnc = MkCombo(ID_ENC, {L"원본과 같게", L"UTF-8 (BOM)", L"UTF-8", L"ANSI (CP949)", L"UTF-16"}, IniGet(L"enc", 0));
    g_chkSync = MkCheck(ID_SYNC, L"클릭한 곳 결과와 연동", IniGet(L"sync", 1) != 0);
    g_btnPaste = MkBtn(ID_PASTE, L"붙여넣기");
    g_btnClear = MkBtn(ID_CLEAR, L"지우기");
    g_btnCopyAll = MkBtn(ID_COPYALL, L"결과 복사");
    g_btnReplace = MkBtn(ID_REPLACE, L"찾기·바꾸기");

    g_btnTidy = MkBtn(ID_MODE_TIDY, L"정리");
    g_btnRead = MkBtn(ID_READ, L"읽기");
    // 읽기 모드의 보기 설정
    g_cbVFont = Mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL, ID_VFONT);
    {
        int sel = 0;
        for (size_t i = 0; i < g_fontNames.size(); ++i) {
            SendMessageW(g_cbVFont, CB_ADDSTRING, 0, (LPARAM)g_fontNames[i].c_str());
            if (g_fontNames[i] == g_vFont) sel = (int)i;
        }
        SendMessageW(g_cbVFont, CB_SETCURSEL, sel, 0);
    }
    SetWindowSubclass(g_cbVFont, ComboProc, 0, 0);
    g_cbVSize = Mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL, ID_VSIZE);
    SetWindowSubclass(g_cbVSize, ComboProc, 0, 0);
    {
        int sel = 2;
        for (int i = 0; i < (int)(sizeof(kSizes) / sizeof(*kSizes)); ++i) {
            SendMessageW(g_cbVSize, CB_ADDSTRING, 0, (LPARAM)std::to_wstring(kSizes[i]).c_str());
            if (kSizes[i] == g_vSize) sel = i;
        }
        SendMessageW(g_cbVSize, CB_SETCURSEL, sel, 0);
    }
    g_cbVColor = MkCombo(ID_VCOLOR, {L"기본 (테마 색)", L"아이보리", L"세피아", L"보호 녹색", L"종이 회색", L"연하늘",
                                     L"저녁 (앰버)", L"어두운 화면", L"사용자 지정"}, g_vColor);
    g_btnVFg = MkBtn(ID_VFG, L"");
    g_btnVBg = MkBtn(ID_VBG, L"");
    g_btnVBold = MkBtn(ID_VBOLD, L"굵게");
    g_cbVLine = MkCombo(ID_VLINE, {L"좁게", L"보통", L"조금 넓게", L"넓게", L"아주 넓게"}, g_vLine);
    g_cbVMargin = MkCombo(ID_VMARGIN, {L"좁게", L"보통", L"넓게"}, g_vMargin);
    g_btnVReset = MkBtn(ID_VRESET, L"보기 초기화");
    g_btnToc = MkBtn(ID_RTOC, L"목차");
    g_btnFind = MkBtn(ID_RFIND, L"찾기");
    g_btnMark = MkBtn(ID_RMARK, L"책갈피");
    g_btnFull = MkBtn(ID_RFULL, L"전체 화면");

    g_src = MkRich(ID_SRC);
    g_dst = MkRich(ID_DST);
    SetWindowSubclass(g_dst, DstProc, 0, 0);
    SendMessageW(g_dst, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_DROPFILES | ENM_SELCHANGE);
    ApplyEditLook(g_src);
    ApplyEditLook(g_dst);
    if (g_reading) { SendMessageW(g_dst, WM_SETFONT, (WPARAM)g_readFont, TRUE); ApplyEditLook(g_dst); SendMessageW(g_dst, EM_SETREADONLY, TRUE, 0); }
    EnableWindow(g_btnSave, FALSE);
    EnableWindow(g_btnSaveAs, FALSE);
    g_fold = IniGet(L"fold", 0) != 0;
    SetWindowTextW(g_btnFold, g_fold ? L"옵션 펼치기" : L"접기");
    SetWindowTextW(g_btnReset, L"초기화");
    {   // 아이콘 단추·선택목록에 풍선 도움말 (마우스를 올리면 이름 표시)
        CreateTipWindow();
        AddTip(g_btnOpen, L"열기  ·  TXT, EPUB", L"Ctrl+O");
        AddTip(g_btnRecent, L"최근 파일");
        AddTip(g_btnBatch, L"여러 파일 정리 · 합치기", L"TXT, EPUB");
        AddTip(g_btnSave, L"저장", L"Ctrl+S");
        AddTip(g_btnSaveAs, L"다른 이름으로 저장", L"Ctrl+Shift+S");
        AddTip(g_btnTheme, L"화면 테마", L"밝게 · 어둡게");
        AddTip(g_btnAbout, L"정보 · 사용법", L"F1");
        AddTip(g_btnReset, L"모든 옵션을 처음 상태로");
        AddTip(g_btnFold, L"옵션 칸 접기 · 펼치기");
        AddTip(g_btnToc, L"목차");
        AddTip(g_btnFind, L"찾기 · 바꾸기", L"Ctrl+F");
        AddTip(g_btnMark, L"책갈피", L"추가 Ctrl+B");
        AddTip(g_btnFull, L"전체 화면", L"F11");
        FullTip();
        AddTip(g_cbVFont, L"글꼴");
        AddTip(g_cbVSize, L"글자 크기", L"Ctrl+휠");
        AddTip(g_btnVBold, L"굵게");
        AddTip(g_cbVColor, L"배색");
        AddTip(g_btnVFg, L"글자색 직접 고르기");
        AddTip(g_btnVBg, L"바탕색 직접 고르기");
        AddTip(g_cbVLine, L"줄 간격");
        AddTip(g_cbVMargin, L"여백");
        AddTip(g_btnVReset, L"보기 설정 초기화");
    }
}

static void Layout() {
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int toolH = g_fullscreen ? 0 : S(46), statusH = g_fullscreen ? 0 : S(26);  // 전체 화면: 도구줄·상태줄 없음
    int rowH = S(29), titleH = S(20), padT = S(2), padB = S(9);
    int blockH = titleH + rowH * 3;  // 그룹 한 줄 높이
    int ch = S(26);                  // 선택목록·입력 칸 높이
    int cbH = ch;                    // 선택목록의 실제 높이 (배율에 따라 1~2px 다를 수 있어 재서 가운데 맞춤)
    { RECT r; if (GetWindowRect(g_cbBlank, &r) && r.bottom - r.top > S(10)) cbH = r.bottom - r.top; }
    HDWP dw = BeginDeferWindowPos(48);
    auto put = [&](HWND h, int x, int y, int w, int hh, bool show = true) {
        dw = DeferWindowPos(dw, h, nullptr, x, y, w, hh, SWP_NOZORDER | SWP_NOACTIVATE | (show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    };
    auto setText = [](HWND h, const wchar_t* t) { if (WinText(h) != t) SetWindowTextW(h, t); };

    // 도구줄: [아이콘 야문 클린리더] [정리|읽기]  파일 이름 ...  [열기▾][여러 파일] | [다른 이름으로 저장][저장] | [☾][ⓘ]
    g_rcTool = {0, 0, W, toolH};
    g_toolSeps.clear();
    if (g_fullscreen) {
        for (HWND hb : {g_btnSave, g_btnSaveAs, g_btnAbout, g_btnBatch, g_btnOpen, g_btnRecent, g_btnTheme, g_btnTidy, g_btnRead}) put(hb, 0, 0, 0, 0, false);
        g_rcModeSeg = {0, 0, 0, 0};
    } else {
        int bx = W - S(8), bh = S(30), by = (toolH - bh) / 2;
        // 오른쪽부터 배치. sepLeft: 이 버튼 왼쪽에 묶음 구분선을 둠
        struct TB { HWND h; bool sepLeft; };
        std::vector<TB> order = {{g_btnAbout, false}, {g_btnTheme, true}, {g_btnSave, false}, {g_btnSaveAs, true}};
        if (!g_reading) order.push_back({g_btnBatch, false});
        order.push_back({g_btnRecent, false});
        order.push_back({g_btnOpen, false});
        if (g_reading) put(g_btnBatch, 0, 0, 0, 0, false);  // 읽기 모드에서는 여러 파일 변환 숨김
        for (const TB& t : order) {
            HFONT f = t.h == g_btnSave ? g_btnBold : g_btnFont;
            int w = MeasureW(f, WinText(t.h)) + S(16) + S(7) + S(22);  // 아이콘 + 간격 + 좌우 여백
            if (t.h == g_btnSave) w += S(10);
            if (t.h == g_btnTheme || t.h == g_btnAbout || t.h == g_btnSave || t.h == g_btnSaveAs) w = S(32);  // 아이콘만
            if (t.h == g_btnRecent) w = S(22);
            if (t.h == g_btnOpen) w -= S(4);  // ▾와 붙여 한 덩어리로 보이게
            bx -= w;
            put(t.h, bx, by, w, bh);
            g_toolBtnLeft = bx;
            bx -= S(2);
            if (t.sepLeft) { bx -= S(6); g_toolSeps.push_back(bx); bx -= S(7); }
        }
        {   // [정리 | 읽기] 모드 전환: 프로그램 이름 옆. 창이 아주 좁을 때만 이름을 숨김
            int w1 = MeasureW(g_btnBold, L"정리") + S(14) + S(6) + S(24);
            int w2 = MeasureW(g_btnBold, L"읽기") + S(14) + S(6) + S(24);
            int segW = w1 + w2 + S(6), sh = S(32), sy = (toolH - sh) / 2;
            int sx = S(42) + MeasureW(g_titleFont, kAppName) + S(14);
            g_showTitle = true;
            if (g_toolBtnLeft - (sx + segW) < S(24)) { g_showTitle = false; sx = S(42); }
            g_rcModeSeg = {sx, sy, sx + segW, sy + sh};
            put(g_btnTidy, sx + S(3), sy + S(3), w1, sh - S(6));
            put(g_btnRead, sx + S(3) + w1, sy + S(3), w2, sh - S(6));
        }
    }

    // 옵션 패널: 도구줄과 같은 바탕에 그룹을 고르게 펼침. 창이 좁으면 다음 줄로 내려감 (읽기 모드에서는 숨김)
    const bool hideOpts = g_fold || g_reading || g_fullscreen;
    g_items.clear();
    g_groupSep.clear();
    int x0 = S(12), avail = W - S(12) - x0, sepGap = S(8);
    int lwA = 0, cwA = 0, lwB = 0, cwB = 0, lwC = 0, cwC = 0, lwE = 0, cwE = 0, ewB = S(46), uwB = 0, chkW1 = 0, chk3a = 0, chk3b = 0;
    auto combosW = [&](std::initializer_list<HWND> hs) {
        int mx = 0;
        for (HWND h : hs) {
            int cnt = (int)SendMessageW(h, CB_GETCOUNT, 0, 0);
            for (int i = 0; i < cnt; ++i) {
                wchar_t buf[128]; int n = (int)SendMessageW(h, CB_GETLBTEXT, i, (LPARAM)buf);
                mx = std::max(mx, MeasureW(g_uiFont, wstring(buf, n > 0 ? n : 0)));
            }
        }
        return mx + S(34);
    };
    auto labelsW = [&](std::initializer_list<const wchar_t*> ls) {
        int mx = 0;
        for (auto l : ls) mx = std::max(mx, MeasureW(g_uiFont, l));
        return mx + S(9);
    };
    auto checkW = [&](HWND h) { return MeasureW(g_uiFont, WinText(h)) + S(26); };
    // 펼친 목록은 늘 가장 긴 항목이 다 보이게 (닫힌 칸보다 좁아지지는 않음: Windows가 칸 폭을 최소로 씀)
    auto dropW = [&](HWND h) {  // 목록이 길어 스크롤 막대가 생기면 그 폭만큼 더
        int sb = SendMessageW(h, CB_GETCOUNT, 0, 0) > 8 ? GetSystemMetrics(SM_CXVSCROLL) : 0;
        SendMessageW(h, CB_SETDROPPEDWIDTH, combosW({h}) + S(4) + sb, 0);
    };
    static bool dropDone = false;  // 목록 항목은 바뀌지 않으므로 한 번만 잼 (창 크기를 바꿀 때 느려지지 않게)
    if (!dropDone) {
        for (HWND h : {g_cbBlank, g_cbJoin, g_cbSpace, g_cbSplit, g_cbScene, g_cbParaSep, g_cbIndent, g_cbQuote, g_cbEll, g_cbReadEnc, g_cbEnc,
                       g_cbVFont, g_cbVSize, g_cbVColor, g_cbVLine, g_cbVMargin}) dropW(h);
        dropDone = true;
    }
    lwA = labelsW({L"빈 줄", L"끊긴 줄", L"띄어쓰기"}); cwA = combosW({g_cbBlank, g_cbJoin, g_cbSpace});
    lwB = labelsW({L"나누기", L"장면 전환", L"문단 사이"}); cwB = combosW({g_cbSplit, g_cbScene, g_cbParaSep});
    uwB = MeasureW(g_uiFont, L"자") + S(4);
    lwC = labelsW({L"들여쓰기", L"따옴표", L"말줄임표"}); cwC = combosW({g_cbIndent, g_cbQuote, g_cbEll});
    chkW1 = std::max(checkW(g_chkCollapse), checkW(g_chkJunk)); chk3a = checkW(g_chkHanja); chk3b = checkW(g_chkHalf);
    lwE = labelsW({L"읽기", L"저장"});
    cwE = combosW({g_cbReadEnc, g_cbEnc});  // 어떤 항목을 골라도 닫힌 칸에서 잘리지 않게
    int miniFold = MeasureW(g_smallFont, L"접기") + S(13) + S(4) + S(14);
    int miniReset = MeasureW(g_smallFont, L"초기화") + S(13) + S(4) + S(14);
    int titleE = MeasureW(g_sectFont, L"파일") + S(10) + miniReset + S(2) + miniFold;
    // 그룹의 기본 폭 (내용에 꼭 맞는 폭)
    int nat[5] = {lwA + cwA, lwB + cwB + S(6) + ewB + S(4) + uwB, lwC + cwC, std::max(chkW1, chk3a + S(14) + chk3b),
                  std::max(lwE + cwE, titleE)};
    // 늘 한 줄: 남는 폭은 선택목록을 넓혀 고르게 채우고(너무 넓어지지는 않게), 모자라면 선택목록만 줄임 (메뉴 자리는 그대로)
    int rowOf[5] = {0, 0, 0, 0, 0}, rows = 1, extra[5] = {0, 0, 0, 0, 0};
    {
        int used = 0;
        for (int i = 0; i < 5; ++i) used += nat[i];
        used += 4 * (sepGap * 2 + 1);
        g_panelNeedW = x0 + used + S(12);  // 옵션이 한 줄에 다 들어가는 창 안쪽 폭 (창 최소 폭으로 씀)
        int spare = avail - used;
        if (spare > 0) {
            int each = std::min(spare / 4, S(90));  // 선택목록이 있는 네 묶음만 넓힘 (체크 칸 묶음은 넓혀도 의미 없음)
            for (int i = 0; i < 5; ++i) if (i != 3) extra[i] = each;
        } else if (spare < 0) {  // 화면이 좁아 최소 폭보다 작을 때만: 선택목록을 조금씩 줄임 (끝은 …로)
            int need = -spare;
            int room[5] = {cwA - S(64), cwB - S(64), cwC - S(64), 0, std::max(cwE, nat[4] - lwE) - std::max(S(64), titleE - lwE)};  // 파일 묶음은 제목 줄([초기화][접기]) 폭은 남김
            int total = 0;
            for (int r : room) total += std::max(0, r);
            for (int i = 0; i < 5 && total > 0; ++i) extra[i] = -std::min(std::max(0, room[i]), (int)((long long)need * std::max(0, room[i]) / total + 1));
        }
    }
    int gx[5];
    {
        int x = x0;
        for (int i = 0; i < 5; ++i) {
            if (i > 0) {
                x += sepGap;
                int gy = toolH + padT;
                g_groupSep.push_back(RECT{x, gy + S(4), x + 1, gy + blockH - S(2)});
                x += 1 + sepGap;
            }
            gx[i] = x;
            x += nat[i] + extra[i];
        }
    }
    auto gyOf = [&](int i) { return toolH + padT + rowOf[i] * (blockH + S(6)); };
    auto title = [&](int i, const wchar_t* t) {
        int gy = gyOf(i);
        g_items.push_back({{gx[i], gy, gx[i] + nat[i] + extra[i], gy + titleH}, t, 0});
    };
    auto label = [&](int i, int row, const wchar_t* t, int lw) {
        int ry = gyOf(i) + titleH + row * rowH;
        g_items.push_back({{gx[i], ry, gx[i] + lw, ry + rowH}, t, 1});
    };
    auto ctrl = [&](int i, HWND h, int row, int cx, int w, bool combo) {
        int y = gyOf(i) + titleH + row * rowH + (rowH - (combo ? cbH : ch)) / 2;
        put(h, cx, y, w, combo ? S(280) : ch, !hideOpts);
    };
    {   // 줄 합치기
        int cw = cwA + extra[0];
        title(0, L"줄 합치기");
        label(0, 0, L"빈 줄", lwA); ctrl(0, g_cbBlank, 0, gx[0] + lwA, cw, true);
        label(0, 1, L"끊긴 줄", lwA); ctrl(0, g_cbJoin, 1, gx[0] + lwA, cw, true);
        label(0, 2, L"띄어쓰기", lwA); ctrl(0, g_cbSpace, 2, gx[0] + lwA, cw, true);
    }
    {   // 문단: 나누기 [목록][기준]자 / 장면 전환 / 문단 사이
        int x = gx[1], full = lwB + cwB + extra[1] + S(6) + ewB + S(4) + uwB;
        title(1, L"문단");
        label(1, 0, L"나누기", lwB); ctrl(1, g_cbSplit, 0, x + lwB, cwB + extra[1], true);
        {   // 글자 수 칸: 둥근 테두리는 Paint에서 그리고 칸은 안쪽에
            int ey = gyOf(1) + titleH + (rowH - cbH) / 2;  // 옆 선택목록과 같은 높이
            int ex = x + lwB + cwB + extra[1] + S(6);
            g_rcLongBox = hideOpts ? RECT{0, 0, 0, 0} : RECT{ex, ey, ex + ewB, ey + cbH};
            int ih = MeasureH(g_uiFont);
            put(g_edLong, ex + S(4), ey + (cbH - ih) / 2, ewB - S(8), ih, !hideOpts);
            int ry = gyOf(1) + titleH;
            g_items.push_back({{ex + ewB + S(4), ry, ex + ewB + S(4) + uwB, ry + rowH}, L"자", 2});
        }
        label(1, 1, L"장면 전환", lwB); ctrl(1, g_cbScene, 1, x + lwB, full - lwB, true);
        label(1, 2, L"문단 사이", lwB); ctrl(1, g_cbParaSep, 2, x + lwB, full - lwB, true);
    }
    {   // 모양
        int cw = cwC + extra[2];
        title(2, L"모양");
        label(2, 0, L"들여쓰기", lwC); ctrl(2, g_cbIndent, 0, gx[2] + lwC, cw, true);
        label(2, 1, L"따옴표", lwC); ctrl(2, g_cbQuote, 1, gx[2] + lwC, cw, true);
        label(2, 2, L"말줄임표", lwC); ctrl(2, g_cbEll, 2, gx[2] + lwC, cw, true);
    }
    {   // 다듬기: 체크 칸은 글자 폭 그대로 (넓혀도 의미 없음)
        title(3, L"다듬기");
        ctrl(3, g_chkCollapse, 0, gx[3], chkW1, false);
        ctrl(3, g_chkJunk, 1, gx[3], chkW1, false);
        ctrl(3, g_chkHanja, 2, gx[3], chk3a, false);
        ctrl(3, g_chkHalf, 2, gx[3] + chk3a + S(14), chk3b, false);
    }
    {   // 파일 (제목 줄 오른쪽에 [초기화] [접기])
        int cw = std::max(cwE, nat[4] - lwE) + extra[4];
        title(4, L"파일");
        label(4, 0, L"읽기", lwE); ctrl(4, g_cbReadEnc, 0, gx[4] + lwE, cw, true);
        label(4, 1, L"저장", lwE); ctrl(4, g_cbEnc, 1, gx[4] + lwE, cw, true);
        int right = gx[4] + lwE + cw, my = gyOf(4) + (titleH - S(22)) / 2;
        if (!g_reading && !g_fullscreen && !g_fold) {
            setText(g_btnFold, L"접기");
            put(g_btnFold, right - miniFold, my, miniFold, S(22));
            put(g_btnReset, right - miniFold - S(2) - miniReset, my, miniReset, S(22));
        }
    }
    int panelH = 0;
    if (!hideOpts) panelH = padT + rows * blockH + (rows - 1) * S(6) + padB;
    else if (g_fold && !g_reading && !g_fullscreen) {  // 접힌 상태: 얇은 줄에 [옵션 펼치기]만
        panelH = S(32);
        setText(g_btnFold, L"옵션 펼치기");
        int fw = MeasureW(g_smallFont, L"옵션 펼치기") + S(13) + S(4) + S(16);
        put(g_btnFold, x0 - S(4), toolH + (panelH - S(24)) / 2 - S(2), fw, S(24));
        put(g_btnReset, 0, 0, 0, 0, false);
    }
    if (g_reading || g_fullscreen) for (HWND hb : {g_btnFold, g_btnReset}) put(hb, 0, 0, 0, 0, false);

    // 읽기 모드: 보기 설정 줄 하나에 읽기 도구까지 (글 칸 머리 줄을 따로 두지 않음)
    // [목차 찾기 책갈피] | 글꼴 [글꼴][크기][B] | [배색][글자색][바탕색] | 줄 간격 [ ] 여백 [ ] [↺] ........ [전체 화면]
    // 전체 화면에서는 이 줄이 숨고, 읽기 도구는 글 칸 머리에 둠
    g_readItems.clear();
    g_viewSeps.clear();
    {
        HWND vc[] = {g_cbVFont, g_cbVSize, g_btnVBold, g_cbVColor, g_btnVFg, g_btnVBg, g_cbVLine, g_cbVMargin, g_btnVReset};
        static int fontW = 0;  // 닫힌 글꼴 칸은 좁게 (펼친 목록은 전체 이름). 한 번만 잼
        if (!fontW) fontW = std::min(combosW({g_cbVFont}), S(150));
        auto labW = [&](const wchar_t* t) { return MeasureW(g_uiFont, t) + S(8); };  // 이름표 글 + 간격
        const int sqW = S(30);             // 아이콘 단추 폭
        const int toolGap = S(2);          // 읽기 도구 단추 사이
        const int sepW = S(10) + 1 + S(10);  // 묶음 사이 (구분선 양옆 같은 간격)
        int sizeW = combosW({g_cbVSize}), colorW = combosW({g_cbVColor}) + S(10), lineW = combosW({g_cbVLine}), marginW = combosW({g_cbVMargin});
        // 보기 설정 줄이 한 줄에 들어가는 폭 (정리 모드에서도 미리 재 둠: 읽기로 바꿨을 때 두 줄이 되지 않게)
        g_viewNeedW = S(12) + sqW * 3 + toolGap * 2 + sepW + labW(L"글꼴") + fontW + S(6) + sizeW + S(6) + sqW + sepW + colorW + S(6) + sqW + S(4) + sqW +
                      sepW + labW(L"줄 간격") + lineW + S(14) + labW(L"여백") + marginW + S(8) + sqW + S(16) + sqW + S(10);
        if (!g_reading || g_fullscreen) {
            for (HWND hv : vc) put(hv, 0, 0, 0, 0, false);
        } else {
            int rx = S(12), rowR = S(40), ry0 = toolH;
            int cy = ry0 + (rowR - cbH) / 2;  // 단추도 선택목록과 같은 높이
            auto label = [&](const wchar_t* t) {
                int lw = labW(t);
                g_readItems.push_back({{rx, cy, rx + lw, cy + cbH}, t, 1});
                rx += lw;
            };
            auto item = [&](HWND hv, int w, bool combo, int gapAfter) {
                put(hv, rx, cy, w, combo ? S(300) : cbH, true);
                rx += w + gapAfter;
            };
            auto sep = [&]() {
                rx += S(10);
                g_viewSeps.push_back(RECT{rx, ry0 + S(10), rx + 1, ry0 + rowR - S(10)});
                rx += 1 + S(10);
            };
            item(g_btnToc, sqW, false, toolGap);
            item(g_btnFind, sqW, false, toolGap);
            item(g_btnMark, sqW, false, 0);
            sep();
            label(L"글꼴");
            item(g_cbVFont, fontW, true, S(6));
            item(g_cbVSize, sizeW, true, S(6));
            item(g_btnVBold, sqW, false, 0);
            sep();
            item(g_cbVColor, colorW, true, S(6));
            item(g_btnVFg, sqW, false, S(4));
            item(g_btnVBg, sqW, false, 0);
            sep();
            label(L"줄 간격");
            item(g_cbVLine, lineW, true, S(14));
            label(L"여백");
            item(g_cbVMargin, marginW, true, S(8));
            item(g_btnVReset, sqW, false, 0);
            put(g_btnFull, W - S(10) - sqW, cy, sqW, cbH, true);  // 전체 화면: 오른쪽 끝
            panelH = rowR;
        }
    }
    g_rcPanel = {0, toolH, W, toolH + panelH};

    // 텍스트 칸: 둘레·칸 사이 여백을 좁게 (종이 두 장)
    int m = S(6), gap = S(6), headH = S(32);
    int top = toolH + panelH + (g_fullscreen ? 0 : m), bottom = H - statusH - m;
    int half = (W - m * 2 - gap) / 2;
    int bh = S(26), by = top + (headH - bh) / 2;
    auto headBtnW = [&](HWND hb) { return MeasureW(g_uiFont, WinText(hb)) + S(14) + S(5) + S(16); };  // 아이콘 + 글
    int edBottomInset = S(4);  // 둥근 아래 모서리가 보이게 편집 칸을 조금 올림 (그 틈은 칸 바탕색으로 칠함)
    g_rcSrcHead = {m, top, m + half, top + headH};
    g_rcDstHead = {m + half + gap, top, W - m, top + headH};
    if (g_reading && !g_fullscreen) {  // 읽기 모드: 결과 창을 왼쪽 끝까지 넓히고, 칸 머리 없이 종이만 (읽기 도구는 보기 설정 줄에)
        g_rcSrcHead = {0, 0, 0, 0};
        g_rcDstHead = {m, top, W - m, top};  // 높이 0: 둥근 종이의 위 끝
        for (HWND hh : {g_btnPaste, g_btnClear, g_chkSync, g_btnCopyAll, g_btnReplace}) put(hh, 0, 0, 0, 0, false);
        put(g_src, 0, 0, 0, 0, false);
        int topInset = S(4);  // 둥근 위 모서리가 보이게 (아래 모서리와 같게)
        put(g_dst, m + 1, top + topInset, W - m * 2 - 2, bottom - top - topInset - 1 - edBottomInset);
    } else if (g_reading) {  // 전체 화면: 보기 설정 줄이 없으므로 칸 머리에 읽기 도구
        g_rcSrcHead = {0, 0, 0, 0};
        g_rcDstHead = {m, top, W - m, top + headH};
        int rx = g_rcDstHead.left + S(12) + MeasureW(g_cardFont, L"읽기") + S(10);
        for (HWND hb : {g_btnToc, g_btnFind, g_btnMark, g_btnFull}) {  // 목차 · 찾기 · 책갈피 · 전체 화면 (아이콘만)
            put(hb, rx, by, S(30), bh);
            rx += S(30) + S(2);
        }
        g_dstMetaLeft = rx + S(8);
        for (HWND hh : {g_btnPaste, g_btnClear, g_chkSync, g_btnCopyAll, g_btnReplace}) put(hh, 0, 0, 0, 0, false);
        put(g_src, 0, 0, 0, 0, false);
        put(g_dst, m + 1, top + headH, W - m * 2 - 2, bottom - top - headH - 1 - edBottomInset);
    } else {
        for (HWND hb : {g_btnToc, g_btnFind, g_btnMark, g_btnFull}) put(hb, 0, 0, 0, 0, false);
        int cx = g_rcSrcHead.left + S(12) + MeasureW(g_cardFont, L"원본") + S(10);
        int w1 = headBtnW(g_btnPaste), w2 = headBtnW(g_btnClear);
        put(g_btnPaste, cx, by, w1, bh); cx += w1 + S(2);
        put(g_btnClear, cx, by, w2, bh); cx += w2 + S(10);
        put(g_chkSync, cx, top + (headH - S(22)) / 2, checkW(g_chkSync), S(22));
        g_srcMetaLeft = cx + checkW(g_chkSync) + S(8);
        int dx = g_rcDstHead.left + S(12) + MeasureW(g_cardFont, L"정리 결과") + S(10);
        int w3 = headBtnW(g_btnCopyAll), w4 = headBtnW(g_btnReplace);
        put(g_btnCopyAll, dx, by, w3, bh);
        put(g_btnReplace, dx + w3 + S(2), by, w4, bh);
        g_dstMetaLeft = dx + w3 + S(2) + w4 + S(8);
        put(g_src, m + 1, top + headH, half - 2, bottom - top - headH - 1 - edBottomInset);
        put(g_dst, m + half + gap + 1, top + headH, W - m - (m + half + gap) - 2, bottom - top - headH - 1 - edBottomInset);
    }
    g_rcStatus = {0, H - statusH, W, H};
    g_rcHeader = g_rcTool;
    EndDeferWindowPos(dw);

    for (HWND e : {g_src, g_dst}) {
        RECT r; GetClientRect(e, &r);
        int ml = S(12), mr = S(10), mt = S(8);
        if (g_reading && e == g_dst) {  // 읽기 모드 여백: 좁게 / 보통 / 넓게
            int ew = r.right - r.left;
            if (g_vMargin == 1) ml = mr = std::max(S(18), ew * 3 / 100), mt = S(12);       // 보통: 양옆 약 3%
            else if (g_vMargin == 2) ml = mr = std::max(S(24), ew * 6 / 100), mt = S(16);  // 넓게: 양옆 약 6%
        }
        r.left += ml; r.right -= mr; r.top += mt;
        RECT old; SendMessageW(e, EM_GETRECT, 0, (LPARAM)&old);
        if (r.right > r.left && r.bottom > r.top) SendMessageW(e, EM_SETRECT, 0, (LPARAM)&r);
        if (memcmp(&old, &r, sizeof(r)) != 0) InvalidateRect(e, nullptr, TRUE);  // 여백이 바뀌면 칸 전체를 다시 그림 (예전 글자 자국 방지)
    }
}

static void RoundPathSel(GraphicsPath& p, REAL x, REAL y, REAL w, REAL h, REAL r, bool tl, bool tr, bool br, bool bl);
static void DrawToolIcon(Graphics& g, int id, REAL ox, REAL oy, REAL u, COLORREF c, COLORREF bg);
static void Paint(HDC hdc) {
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    HDC dc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W > 0 ? W : 1, H > 0 ? H : 1);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    // 머리(도구줄 + 옵션 패널 / 보기 설정 줄)는 한 바탕, 그 아래 글 칸 둘레는 옅은 회색
    FillRect2(dc, rc, col::head);
    int headBottom = g_rcPanel.bottom;
    {
        RECT body = {0, headBottom, W, g_rcStatus.top};
        FillRect2(dc, body, col::body);
        if (!g_fullscreen) { RECT l = {0, headBottom - 1, W, headBottom}; FillRect2(dc, l, col::panelLine); }
    }
    if (!g_reading && !g_fold) for (auto& sep : g_groupSep) FillRect2(dc, sep, col::toolSep);
    if (g_reading) for (auto& sep : g_viewSeps) FillRect2(dc, sep, col::toolSep);
    for (int x : g_toolSeps) { RECT l = {x, (g_rcTool.bottom - S(20)) / 2, x + 1, (g_rcTool.bottom + S(20)) / 2}; FillRect2(dc, l, col::toolSep); }

    // 글 칸: 둥근 종이 (머리 + 편집 칸 + 아래 둥근 모서리)
    auto paneFrame = [&](const RECT& head, HWND ed, COLORREF edBg) {
        RECT er; GetWindowRect(ed, &er);
        MapWindowPoints(nullptr, g_hwnd, (POINT*)&er, 2);
        RECT frame = {head.left, head.top, head.right, er.bottom + 1 + S(4)};
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillRound(g, frame, S(6), edBg, col::border);
        if (head.bottom <= head.top) return;  // 칸 머리 없는 종이 (읽기 모드)
        GraphicsPath p;
        RoundPathSel(p, (REAL)head.left + 1, (REAL)head.top + 1, (REAL)(head.right - head.left - 2), (REAL)(head.bottom - head.top - 1), (REAL)S(5), true, true, false, false);
        SolidBrush hb(GC(col::paneHead));
        g.FillPath(&hb, &p);
        g.Flush();
        RECT hl = {head.left + 1, head.bottom - 1, head.right - 1, head.bottom};
        FillRect2(dc, hl, col::panelLine);
    };
    COLORREF dstBg = col::editBg;
    if (g_reading) { COLORREF fg; ViewColors(fg, dstBg); }
    if (!g_reading) paneFrame(g_rcSrcHead, g_src, col::editBg);
    paneFrame(g_rcDstHead, g_dst, dstBg);
    FillRect2(dc, g_rcStatus, col::status);
    RECT sl = {0, g_rcStatus.top, W, g_rcStatus.top + 1};
    if (!g_fullscreen) FillRect2(dc, sl, col::panelLine);

    // 상태줄 칩 위치 계산 (오른쪽 정렬)
    g_chipRc.clear();
    g_chipIdx.clear();
    {
        int cx = W - S(12), ch = S(19), cy = g_rcStatus.top + (g_rcStatus.bottom - g_rcStatus.top - ch) / 2 + 1;
        int limit = W / 2;
        for (int i = (int)g_chips.size() - 1; i >= 0; --i) {
            int w = MeasureW(g_smallFont, g_chips[i]) + S(16);
            if (cx - w < limit) break;
            g_chipRc.insert(g_chipRc.begin(), RECT{cx - w, cy, cx, cy + ch});
            g_chipIdx.insert(g_chipIdx.begin(), i);
            cx -= w + S(5);
        }
    }
    if (g_fullscreen || g_reading) { g_chipRc.clear(); g_chipIdx.clear(); }  // 정리 결과 칩은 정리 모드에서만
    {
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        for (auto& r : g_chipRc) FillRound(g, r, S(9), col::chipBg);
    }

    // 도구줄
    if (!g_fullscreen) {
    HICON ico = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, S(24), S(24), 0);
    if (ico) { DrawIconEx(dc, S(12), (g_rcTool.bottom - S(24)) / 2, ico, S(24), S(24), 0, nullptr, DI_NORMAL); DestroyIcon(ico); }
    if (g_showTitle) {
        RECT tr = {S(42), 0, S(42) + S(200), g_rcTool.bottom};
        DrawTxt(dc, kAppName, tr, g_titleFont, col::text);
    }
    {   // 모드 전환 묶음 바탕
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillRound(g, g_rcModeSeg, S(8), col::segBg, col::segLine);
    }
    RECT fr = {g_rcModeSeg.right + S(14), 0, g_toolBtnLeft - S(12), g_rcTool.bottom};
    if (g_path.empty()) {  // 안내 글은 다 들어갈 때만 (잘린 글자가 남지 않게)
        const wchar_t* hint = g_text.empty() ? L"파일을 끌어다 놓거나 [열기]" : L"붙여넣은 글";
        if (MeasureW(g_uiFont, hint) <= fr.right - fr.left)
            DrawTxt(dc, hint, fr, g_uiFont, col::faint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else if (fr.right - fr.left > S(48)) {  // 파일 이름 + 그 아래 폴더 위치
        int mid = g_rcTool.bottom / 2;
        RECT r1 = {fr.left, mid - S(18), fr.right, mid + S(1)}, r2 = {fr.left, mid + S(1), fr.right, mid + S(17)};
        wstring name = FileName(g_path), dir = g_path.substr(0, g_path.size() - name.size());
        if (!g_mergeFiles.empty()) name += L"  (" + std::to_wstring(g_mergeFiles.size()) + L"개 파일 합침)";
        DrawTxt(dc, name, r1, g_uiFont, col::text, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        DrawTxt(dc, dir, r2, g_smallFont, col::faint, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
    }
    }  // !g_fullscreen

    // 옵션 글자
    if (!g_reading && !g_fold && !g_fullscreen) {  // 글자 수 칸 테두리
        bool en = IsWindowEnabled(g_edLong) != 0;
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillRound(g, g_rcLongBox, S(5), en ? col::field : col::fieldDis,
                  !en ? col::panelLine : GetFocus() == g_edLong ? col::accent : col::border);
    }
    if (g_reading) {
        for (auto& it : g_readItems) DrawTxt(dc, it.text, it.rc, g_uiFont, col::label);  // 이름표 (누르는 단추와 구별되게 글자로)
    } else if (!g_fold && !g_fullscreen) {
        for (auto& it : g_items) {
            if (it.kind == 0) DrawTxt(dc, it.text, it.rc, g_sectFont, col::groupTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            else DrawTxt(dc, it.text, it.rc, g_uiFont, it.kind == 1 ? col::label : col::faint);
        }
    }

    // 칸 머리
    auto head = [&](const RECT& r, const wchar_t* title, const wstring& meta, int metaLeft) {
        RECT t = {r.left + S(12), r.top, r.right - S(12), r.bottom};
        DrawTxt(dc, title, t, g_cardFont, col::text);
        RECT mr = {std::max<int>(metaLeft, t.left), r.top, t.right, r.bottom};
        DrawTxt(dc, meta, mr, g_smallFont, col::faint, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    };
    // 읽기 모드 안내·파일 크기·읽은 %: 전체 화면은 칸 머리에, 아니면 상태줄 오른쪽에 옅게
    wstring readMeta;
    if (g_reading) {
        readMeta = L"클릭·Space: 쪽 넘김   ·   Esc: " + wstring(g_fullscreen ? L"전체 화면 끝" : L"정리 모드");
        if (!g_sizeText.empty()) readMeta += L"   ·   " + g_sizeText;  // 파일 크기 (읽은 %는 맨 오른쪽)
        if (g_readPct >= 0) readMeta += L"   ·   " + std::to_wstring(g_readPct) + L"%";
    }
    if (g_reading) {
        if (g_fullscreen) head(g_rcDstHead, L"읽기", readMeta, g_dstMetaLeft);
    } else {
        head(g_rcSrcHead, L"원본", g_metaSrc, g_srcMetaLeft);
        head(g_rcDstHead, L"정리 결과", g_metaDst.empty() ? wstring(L"직접 고친 내용도 그대로 저장됩니다") : g_metaDst, g_dstMetaLeft);
    }

    // 상태줄
    int msgRight = g_chipRc.empty() ? W - S(14) : g_chipRc.front().left - S(10);
    if (g_reading && !g_fullscreen) {  // 오른쪽: 읽기 안내·크기·% (정리 결과 칩은 정리 모드에서만)
        int mw = MeasureW(g_smallFont, readMeta);
        RECT mr = {std::max<int>(W / 2, W - S(14) - mw), g_rcStatus.top, W - S(14), g_rcStatus.bottom};
        DrawTxt(dc, readMeta, mr, g_smallFont, col::faint, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        msgRight = mr.left - S(16);
    }
    RECT st = {S(14), g_rcStatus.top, msgRight, g_rcStatus.bottom};
    DrawTxt(dc, g_statusMsg, st, g_statusOk ? g_sectFont : g_smallFont, g_statusOk ? col::okText : col::sub);
    for (size_t k = 0; k < g_chipRc.size(); ++k)
        DrawTxt(dc, g_chips[g_chipIdx[k]], g_chipRc[k], g_smallFont, col::chipText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    BitBlt(hdc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

// 모서리를 골라서 둥글게: tl, tr, br, bl
static void RoundPathSel(GraphicsPath& p, REAL x, REAL y, REAL w, REAL h, REAL r, bool tl, bool tr, bool br, bool bl) {
    REAL d = r * 2;
    if (tl) p.AddArc(x, y, d, d, 180, 90); else p.AddLine(x, y, x, y);
    if (tr) p.AddArc(x + w - d, y, d, d, 270, 90); else p.AddLine(x + w, y, x + w, y);
    if (br) p.AddArc(x + w - d, y + h - d, d, d, 0, 90); else p.AddLine(x + w, y + h, x + w, y + h);
    if (bl) p.AddArc(x, y + h - d, d, d, 90, 90); else p.AddLine(x, y + h, x, y + h);
    p.CloseFigure();
}
// 도구줄 아이콘 (16x16 기준 좌표를 box에 맞춰 그림)
static void DrawToolIcon(Graphics& g, int id, REAL ox, REAL oy, REAL u, COLORREF c, COLORREF bg) {
    Pen pen(GC(c), 1.6f * u);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    SolidBrush bgb(GC(bg)), fg(GC(c));
    auto P = [&](REAL x, REAL y) { return PointF(ox + x * u, oy + y * u); };
    switch (id) {
    case ID_OPEN: {  // 폴더
        PointF pts[] = {P(1.5f, 4), P(6, 4), P(7.5f, 5.5f), P(14.5f, 5.5f), P(14.5f, 13), P(1.5f, 13)};
        g.DrawPolygon(&pen, pts, 6);
        break;
    }
    case ID_BATCH: {  // 겹친 문서
        g.DrawRectangle(&pen, ox + 5.5f * u, oy + 1.5f * u, 8.5f * u, 10 * u);
        g.FillRectangle(&bgb, ox + 2 * u, oy + 4.5f * u, 8.5f * u, 10 * u);
        g.DrawRectangle(&pen, ox + 2 * u, oy + 4.5f * u, 8.5f * u, 10 * u);
        break;
    }
    case ID_FOLD: {  // 접기 ▲ / 펼치기 ▼
        if (!g_fold) { PointF pts[] = {P(3, 10.5f), P(8, 5.5f), P(13, 10.5f)}; g.DrawLines(&pen, pts, 3); }
        else { PointF pts[] = {P(3, 5.5f), P(8, 10.5f), P(13, 5.5f)}; g.DrawLines(&pen, pts, 3); }
        break;
    }
    case ID_RESET: case ID_VRESET: {  // 되돌리기 화살표
        g.DrawArc(&pen, ox + 2.5f * u, oy + 2.5f * u, 11 * u, 11 * u, 40, 290);
        PointF pts[] = {P(9.8f, 1.8f), P(12.9f, 3.6f), P(11.2f, 6.8f)};
        g.DrawLines(&pen, pts, 3);
        break;
    }
    case ID_RECENT: {  // ▾
        PointF pts[] = {P(4.5f, 6.5f), P(8, 10), P(11.5f, 6.5f)};
        g.DrawLines(&pen, pts, 3);
        break;
    }
    case ID_THEME: {  // 초승달
        g.FillEllipse(&fg, ox + 2 * u, oy + 2 * u, 12 * u, 12 * u);
        g.FillEllipse(&bgb, ox + 5.6f * u, oy + 0.2f * u, 10.5f * u, 10.5f * u);
        break;
    }
    case ID_ABOUT: {  // (i)
        g.DrawEllipse(&pen, ox + 1.5f * u, oy + 1.5f * u, 13 * u, 13 * u);
        g.FillEllipse(&fg, ox + 7.05f * u, oy + 4.1f * u, 1.9f * u, 1.9f * u);
        g.DrawLine(&pen, P(8, 7.6f), P(8, 11.6f));
        break;
    }
    case ID_PASTE: {  // 클립보드
        GraphicsPath p; RoundPath(p, ox + 3 * u, oy + 2.8f * u, 10 * u, 11.7f * u, 1.4f * u); g.DrawPath(&pen, &p);
        g.FillRectangle(&bgb, ox + 5.5f * u, oy + 1.2f * u, 5 * u, 3.2f * u);
        g.DrawRectangle(&pen, ox + 5.5f * u, oy + 1.2f * u, 5 * u, 3.2f * u);
        break;
    }
    case ID_CLEAR: {  // 휴지통
        g.DrawLine(&pen, P(2.5f, 4), P(13.5f, 4));
        PointF lid[] = {P(6, 4), P(6, 2.2f), P(10, 2.2f), P(10, 4)}; g.DrawLines(&pen, lid, 4);
        PointF cup[] = {P(4, 4), P(4.8f, 14), P(11.2f, 14), P(12, 4)}; g.DrawLines(&pen, cup, 4);
        break;
    }
    case ID_COPYALL: {  // 겹친 네모
        GraphicsPath p1; RoundPath(p1, ox + 5.5f * u, oy + 5.5f * u, 8.5f * u, 8.5f * u, 1.3f * u); g.DrawPath(&pen, &p1);
        PointF bk[] = {P(10.5f, 5.5f), P(10.5f, 2), P(2, 2), P(2, 10.5f), P(5.5f, 10.5f)}; g.DrawLines(&pen, bk, 5);
        break;
    }
    case ID_REPLACE: case ID_RFIND: {  // 돋보기
        g.DrawEllipse(&pen, ox + 2 * u, oy + 2 * u, 8.5f * u, 8.5f * u);
        g.DrawLine(&pen, P(9.6f, 9.6f), P(14, 14));
        break;
    }
    case ID_RTOC: {  // 목록
        for (int i = 0; i < 3; ++i) {
            REAL y = 3.5f + i * 4.5f;
            g.FillEllipse(&fg, ox + 1.3f * u, oy + (y - 0.9f) * u, 1.8f * u, 1.8f * u);
            g.DrawLine(&pen, P(5.5f, y), P(14.5f, y));
        }
        break;
    }
    case ID_RMARK: {  // 책갈피
        PointF bm[] = {P(4, 1.8f), P(12, 1.8f), P(12, 14.2f), P(8, 11), P(4, 14.2f)}; g.DrawPolygon(&pen, bm, 5);
        break;
    }
    case ID_RFULL: {  // 네 귀퉁이
        PointF a1[] = {P(2, 6), P(2, 2), P(6, 2)}, a2[] = {P(10, 2), P(14, 2), P(14, 6)}, a3[] = {P(14, 10), P(14, 14), P(10, 14)}, a4[] = {P(6, 14), P(2, 14), P(2, 10)};
        if (g_fullscreen) {  // 끝내기: 안쪽으로 모인 귀퉁이
            PointF b1[] = {P(2, 6), P(6, 6), P(6, 2)}, b2[] = {P(10, 2), P(10, 6), P(14, 6)}, b3[] = {P(14, 10), P(10, 10), P(10, 14)}, b4[] = {P(6, 14), P(6, 10), P(2, 10)};
            g.DrawLines(&pen, b1, 3); g.DrawLines(&pen, b2, 3); g.DrawLines(&pen, b3, 3); g.DrawLines(&pen, b4, 3);
        } else { g.DrawLines(&pen, a1, 3); g.DrawLines(&pen, a2, 3); g.DrawLines(&pen, a3, 3); g.DrawLines(&pen, a4, 3); }
        break;
    }
    case ID_SAVE: {  // 디스켓
        GraphicsPath path;
        RoundPath(path, ox + 2 * u, oy + 2 * u, 12 * u, 12 * u, 1.6f * u);
        g.DrawPath(&pen, &path);
        g.DrawRectangle(&pen, ox + 5 * u, oy + 2 * u, 5.5f * u, 3.8f * u);
        g.DrawRectangle(&pen, ox + 4.5f * u, oy + 9 * u, 7 * u, 5 * u);
        break;
    }
    case ID_SAVEAS: {  // 겹친 디스켓 두 장 (뒤 장은 오른쪽 위)
        GraphicsPath back;
        RoundPath(back, ox + 5.5f * u, oy + 0.8f * u, 9.7f * u, 9.7f * u, 1.4f * u);
        g.DrawPath(&pen, &back);
        GraphicsPath front;
        RoundPath(front, ox + 0.8f * u, oy + 5.5f * u, 9.7f * u, 9.7f * u, 1.4f * u);
        g.FillPath(&bgb, &front);
        g.DrawPath(&pen, &front);
        g.DrawRectangle(&pen, ox + 3.3f * u, oy + 5.5f * u, 4.4f * u, 2.8f * u);
        g.DrawRectangle(&pen, ox + 3 * u, oy + 11 * u, 5.2f * u, 4.2f * u);
        break;
    }
    case ID_VBG: {  // 바탕색: 페인트 통
        PointF bucket[] = {P(2, 7.5f), P(7, 2.5f), P(12, 7.5f), P(7, 12.5f)};
        g.DrawPolygon(&pen, bucket, 4);
        g.DrawLine(&pen, P(4.5f, 5), P(9.5f, 10));
        g.FillEllipse(&fg, ox + 12.2f * u, oy + 9.5f * u, 2.8f * u, 3.6f * u);
        break;
    }
    }
}

// 체크 표시
static void DrawCheckMark(Graphics& g, const RECT& b, COLORREF c) {
    Pen pen(GC(c), (REAL)S(2));
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    REAL w = (REAL)(b.right - b.left);
    PointF pts[3] = {PointF(b.left + w * 0.24f, b.top + w * 0.52f), PointF(b.left + w * 0.43f, b.top + w * 0.70f),
                     PointF(b.left + w * 0.77f, b.top + w * 0.32f)};
    g.DrawLines(&pen, pts, 3);
}

static void DrawButton(const DRAWITEMSTRUCT* d) {
    HWND h = d->hwndItem;
    RECT rc = d->rcItem;
    int w = rc.right - rc.left, hh = rc.bottom - rc.top;
    HDC dc = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, hh);
    HGDIOBJ ob = SelectObject(dc, bmp);
    RECT r = {0, 0, w, hh};
    int id = (int)d->CtlID;
    bool inPanel = (id >= ID_RP_FIND && id < ID_REPLACE);  // 찾기·바꾸기 창, 목록 창 (옅은 바탕)
    bool headBtn = id == ID_RTOC || id == ID_RFIND || id == ID_RMARK || id == ID_RFULL;
    bool headGhost = id == ID_PASTE || id == ID_CLEAR || id == ID_COPYALL || id == ID_REPLACE || id == ID_RTOC || id == ID_RFIND || id == ID_RMARK || id == ID_RFULL;
    bool mini = id == ID_FOLD || id == ID_RESET || id == ID_VRESET;  // 옵션·보기 줄의 작은 글자 단추
    bool onHead = id == ID_OPEN || id == ID_RECENT || id == ID_BATCH || id == ID_SAVEAS || id == ID_SAVE || id == ID_THEME || id == ID_ABOUT || mini ||
                  id == ID_COLLAPSE || id == ID_JUNK || id == ID_HANJA || id == ID_HALF || id == ID_VFG || id == ID_VBG || id == ID_VBOLD || id == ID_MODE_TIDY || id == ID_READ;
    COLORREF bg = (headBtn && g_reading && !g_fullscreen) ? col::head : (id == ID_SYNC || headGhost || headBtn) ? col::paneHead : inPanel ? col::panel : onHead ? col::head : col::win;  // 주 창 메뉴 영역은 head 바탕
    FillRect2(dc, r, bg);
    bool hover = (GetWindowLongPtrW(h, GWLP_USERDATA) & 1) != 0;
    bool down = (d->itemState & ODS_SELECTED) != 0;
    bool disabled = (d->itemState & ODS_DISABLED) != 0;
    wstring text = WinText(h);
    if (IsCheck(id)) {
        bool on = Checked(h);
        int bs = S(15);
        RECT box = {S(1), (hh - bs) / 2, S(1) + bs, (hh - bs) / 2 + bs};
        {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            if (on) {
                FillRound(g, box, S(3), col::field, hover ? col::checkMark : col::checkBorder);
                DrawCheckMark(g, box, col::checkMark);
            } else {
                FillRound(g, box, S(3), col::field, hover ? col::checkBorder : col::boxBorder);
            }
        }
        RECT tr = {box.right + S(7), 0, w, hh};
        DrawTxt(dc, text, tr, g_uiFont, col::text);
    } else if (id == ID_READ || id == ID_MODE_TIDY) {  // [정리 | 읽기] 묶음의 한 칸
        bool active = (id == ID_READ) == g_reading;
        FillRect2(dc, r, col::segBg);
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        if (active) FillRound(g, r, S(6), col::segActive, col::segActiveEdge);
        else if (hover) FillRound(g, r, S(6), col::segHover);
        COLORREF ic = active ? col::accent : hover ? col::sub : col::faint;
        COLORREF tc = active ? col::segActiveText : hover ? col::text : col::sub;
        HFONT f = active ? g_btnBold : g_btnFont;
        REAL u = (REAL)g_dpi / 96.0f;
        int tw = MeasureW(f, text), iw = S(14), gap = S(6);
        int x0 = (w - (iw + gap + tw)) / 2;
        REAL ox = (REAL)x0, oy = (REAL)(hh - iw) / 2;
        Pen pen(GC(ic), 1.5f * u);
        pen.SetLineJoin(LineJoinRound); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
        if (id == ID_MODE_TIDY) {  // 정리: 고른 줄
            g.DrawLine(&pen, PointF(ox + 1 * u, oy + 3 * u), PointF(ox + 13 * u, oy + 3 * u));
            g.DrawLine(&pen, PointF(ox + 1 * u, oy + 7 * u), PointF(ox + 13 * u, oy + 7 * u));
            g.DrawLine(&pen, PointF(ox + 1 * u, oy + 11 * u), PointF(ox + 8.5f * u, oy + 11 * u));
        } else {                   // 읽기: 펼친 책
            PointF lp[4] = {PointF(ox + 7 * u, oy + 3.5f * u), PointF(ox + 4 * u, oy + 2 * u), PointF(ox + 0.8f * u, oy + 2.5f * u), PointF(ox + 0.8f * u, oy + 11.5f * u)};
            g.DrawLines(&pen, lp, 4);
            PointF lb[3] = {PointF(ox + 0.8f * u, oy + 11.5f * u), PointF(ox + 4 * u, oy + 11 * u), PointF(ox + 7 * u, oy + 12.5f * u)};
            g.DrawLines(&pen, lb, 3);
            PointF rp[4] = {PointF(ox + 7 * u, oy + 3.5f * u), PointF(ox + 10 * u, oy + 2 * u), PointF(ox + 13.2f * u, oy + 2.5f * u), PointF(ox + 13.2f * u, oy + 11.5f * u)};
            g.DrawLines(&pen, rp, 4);
            PointF rb[3] = {PointF(ox + 13.2f * u, oy + 11.5f * u), PointF(ox + 10 * u, oy + 11 * u), PointF(ox + 7 * u, oy + 12.5f * u)};
            g.DrawLines(&pen, rb, 3);
            g.DrawLine(&pen, PointF(ox + 7 * u, oy + 3.5f * u), PointF(ox + 7 * u, oy + 12.5f * u));
        }
        g.Flush();
        HDC gdc = g.GetHDC();
        RECT tr = {x0 + iw + gap, 0, w, hh};
        DrawTxt(gdc, text, tr, f, tc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        g.ReleaseHDC(gdc);
    } else if (id == ID_VFG || id == ID_VBG) {  // 글자색("가") / 바탕색(페인트 통) 아이콘 + 아래에 지금 색 막대
        COLORREF fg, bgc;
        ViewColors(fg, bgc);
        COLORREF fill = down ? col::tbDown : hover ? col::tbHover : col::field;
        {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            FillRound(g, r, S(5), fill, hover ? col::hoverBorder : col::border);
            RECT bar = {S(7), hh - S(8), w - S(7), hh - S(4)};
            FillRound(g, bar, S(1), id == ID_VFG ? fg : bgc, col::swatchEdge);
            if (id == ID_VBG) {
                int iw = S(13);
                DrawToolIcon(g, ID_VBG, (REAL)(w - iw) / 2, (REAL)(hh - S(8) - iw) / 2 + S(1), (REAL)iw / 16.0f, col::tbIcon, fill);
            }
        }
        if (id == ID_VFG) {
            RECT tr = {0, 0, w, hh - S(7)};
            DrawTxt(dc, L"가", tr, g_btnBold, col::text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    } else if (id == ID_VBOLD) {  // 굵게: 켜지면 모드 전환 단추처럼 강조
        {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            if (g_vBold) FillRound(g, r, S(5), col::segActive, col::accent);
            else FillRound(g, r, S(5), down ? col::tbDown : hover ? col::tbHover : col::field, hover ? col::hoverBorder : col::border);
        }
        DrawTxt(dc, L"B", r, g_titleFont, g_vBold ? col::accent : col::text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else if (id == ID_OPEN || id == ID_BATCH || id == ID_ABOUT || id == ID_SAVEAS || id == ID_SAVE || id == ID_THEME || id == ID_RECENT || mini || headGhost) {
        // 도구줄·칸 머리·작은 글자 단추: 평소엔 테두리 없이 아이콘 + 글, 마우스를 올리면 옅은 바탕 ([저장]만 색 채움)
        bool primary = id == ID_SAVE && (!g_reading || g_savedFlash);  // 파란 강조는 정리 모드에서만 (읽기 모드는 다른 아이콘과 같게)
        bool iconOnly = id == ID_THEME || id == ID_ABOUT || id == ID_RECENT || id == ID_SAVE || id == ID_SAVEAS || id == ID_VRESET ||
                        id == ID_RTOC || id == ID_RFIND || id == ID_RMARK || id == ID_RFULL;
        bool L = id != ID_RECENT, R = id != ID_OPEN;  // [열기][▾]는 붙은 한 덩어리
        COLORREF fill, tc, ic;
        if (primary && g_savedFlash) {
            fill = col::savedFill;   // 저장 완료: 초록
            tc = ic = RGB(255, 255, 255);
        } else if (primary) {
            fill = disabled ? col::accentDis : down ? col::accentDown : hover ? col::accentHover : col::accent;
            tc = ic = disabled ? col::accentDisText : RGB(255, 255, 255);
        } else {
            fill = down ? col::tbDown : hover ? col::tbHover : bg;
            tc = disabled ? col::faint : mini ? col::sub : col::text;
            ic = disabled ? col::faint : (mini && !hover) ? col::sub : col::tbIcon;
        }
        {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            if (fill != bg) {
                GraphicsPath path;
                RoundPathSel(path, 0, 0, (REAL)w, (REAL)hh, (REAL)S(6), L, R, R, L);
                SolidBrush b(GC(fill));
                g.FillPath(&b, &path);
            }
            HFONT f = primary ? g_btnBold : mini ? g_smallFont : headGhost ? g_uiFont : g_btnFont;
            if (primary && g_savedFlash) text = L"저장됨";
            if (iconOnly) text.clear();
            int iw = iconOnly ? S(16) : mini ? S(13) : headGhost ? S(14) : S(16);
            int tw = text.empty() ? 0 : MeasureW(f, text), gap = text.empty() ? 0 : (mini || headGhost) ? S(5) : S(7);
            int total = iw + gap + tw, x0 = (w - total) / 2;
            REAL u = (REAL)iw / 16.0f;  // 16 칸 기준 아이콘을 iw 크기로
            if (primary && g_savedFlash) {  // 체크 표시
                REAL oy = (REAL)(hh - iw) / 2;
                Pen pen(GC(ic), 2.0f * u);
                pen.SetLineJoin(LineJoinRound); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
                PointF pts[3] = {PointF(x0 + 2.5f * u, oy + 8.5f * u), PointF(x0 + 6.5f * u, oy + 12.5f * u), PointF(x0 + 13.5f * u, oy + 4 * u)};
                g.DrawLines(&pen, pts, 3);
            } else
                DrawToolIcon(g, id, (REAL)x0, (REAL)(hh - iw) / 2, u, ic, fill);
            if (!text.empty()) {
                RECT tr = {x0 + iw + gap, 0, w, hh};
                g.Flush();
                HDC gdc = g.GetHDC();
                DrawTxt(gdc, text, tr, f, tc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                g.ReleaseHDC(gdc);
            }
        }
    } else {
        bool primary = id == ID_SAVE || id == ID_ABOUT_OK || id == ID_BW_START || id == ID_RP_NEXT || id == ID_RL_OK;
        {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            if (primary) {
                COLORREF c = disabled ? col::accentDis : down ? col::accentDown : hover ? col::accentHover : col::accent;
                FillRound(g, r, S(6), c);
            } else {
                COLORREF c = down ? col::btnDown : hover ? col::hover : col::btnFill;
                FillRound(g, r, S(6), c, hover ? col::hoverBorder : col::border);
            }
        }
        COLORREF tc = primary ? (disabled ? col::accentDisText : RGB(255, 255, 255)) : disabled ? col::faint : col::text;
        HFONT f = (id == ID_READ || headBtn) ? g_uiFont : (primary ? g_btnBold : g_btnFont);
        DrawTxt(dc, text, r, f, tc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    BitBlt(d->hDC, rc.left, rc.top, w, hh, dc, 0, 0, SRCCOPY);
    SelectObject(dc, ob);
    DeleteObject(bmp);
    DeleteDC(dc);
}

// 다듬은 선택목록 항목
static void DrawComboItem(const DRAWITEMSTRUCT* d) {
    if (d->itemID == (UINT)-1) { FillRect2(d->hDC, d->rcItem, col::field); return; }
    wchar_t buf[128];
    int n = (int)SendMessageW(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)buf);
    bool editPart = (d->itemState & ODS_COMBOBOXEDIT) != 0;
    bool sel = (d->itemState & ODS_SELECTED) != 0 && !editPart;
    if (d->hwndItem == g_cbVColor) {  // 배색: 색 견본 (고른 항목은 강조 테두리)
        FillRect2(d->hDC, d->rcItem, sel ? col::selBg : col::field);
        RECT cr = {d->rcItem.left + S(4), d->rcItem.top + S(2), d->rcItem.right - S(4), d->rcItem.bottom - S(2)};
        DrawColorChip(d->hDC, cr, (int)d->itemID, sel ? col::accent : col::swatchEdge);
        return;
    }
    FillRect2(d->hDC, d->rcItem, sel ? col::selBg : col::field);
    RECT r = d->rcItem;
    r.left += S(8);
    DrawTxt(d->hDC, wstring(buf, n > 0 ? n : 0), r, g_uiFont, sel ? col::segActiveText : col::text);
}


// ---------------------------------------------------------------------------
// 여러 파일 창: 목록·순서 정하기 → 파일마다 정리해 저장 / 순서대로 합쳐서 열기
#define WM_BW_STEP (WM_APP + 30)
#define WM_BW_DONE (WM_APP + 31)
struct BwItem { wstring path; long long size = 0; wstring status; };
static std::vector<BwItem> g_bw;
static HWND g_bwWnd, g_bwList, g_bwMode, g_bwDest, g_bwProg;
static wstring g_bwDir;          // 다른 폴더에 저장할 때 그 폴더
static wstring g_bwMsg;          // 창 아래 안내·진행 글
static wstring g_bwLastDir;      // 마지막으로 저장한 폴더 (폴더 열기)
static bool g_bwRunning = false;
static bool g_bwPicking = false;  // 폴더 고르는 중 (알림이 두 번 와도 한 번만)
static volatile LONG g_bwCancel = 0;

static wstring SizeText(long long b) {
    wchar_t buf[32];
    if (b >= 1024 * 1024) swprintf(buf, 32, L"%.1f MB", b / 1048576.0);
    else if (b >= 1024) swprintf(buf, 32, L"%lld KB", (b + 1023) / 1024);
    else swprintf(buf, 32, L"%lld B", b);
    return buf;
}
static wstring DirOf(const wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == wstring::npos ? L"" : p.substr(0, s);
}
static wstring BwOutPath(const wstring& f) {
    wstring o = OutPath(f);
    if (CbSel(g_bwDest) == 1 && !g_bwDir.empty()) o = g_bwDir + L"\\" + FileName(o);
    return o;
}
static std::vector<int> BwSelected() {
    std::vector<int> v;
    int i = -1;
    while ((i = (int)SendMessageW(g_bwList, LVM_GETNEXTITEM, i, LVNI_SELECTED)) >= 0) v.push_back(i);
    return v;
}
static void BwRefresh(const std::vector<int>& sel = {}) {
    SendMessageW(g_bwList, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g_bwList, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < g_bw.size(); ++i) {
        wstring num = std::to_wstring(i + 1), name = FileName(g_bw[i].path), sz = SizeText(g_bw[i].size), dir = DirOf(g_bw[i].path);
        LVITEMW it = {};
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.pszText = (LPWSTR)num.c_str();
        SendMessageW(g_bwList, LVM_INSERTITEMW, 0, (LPARAM)&it);
        auto setSub = [&](int sub, const wstring& t) {
            LVITEMW s2 = {}; s2.iSubItem = sub; s2.pszText = (LPWSTR)t.c_str();
            SendMessageW(g_bwList, LVM_SETITEMTEXTW, i, (LPARAM)&s2);
        };
        setSub(1, name); setSub(2, sz); setSub(3, g_bw[i].status); setSub(4, dir);
    }
    for (int i : sel) {
        LVITEMW it = {}; it.stateMask = LVIS_SELECTED | LVIS_FOCUSED; it.state = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageW(g_bwList, LVM_SETITEMSTATE, i, (LPARAM)&it);
    }
    if (!sel.empty()) SendMessageW(g_bwList, LVM_ENSUREVISIBLE, sel.front(), FALSE);
    SendMessageW(g_bwList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_bwList, nullptr, TRUE);
}
static void BwSetMsg(const wstring& m) { g_bwMsg = m; if (g_bwWnd) InvalidateRect(g_bwWnd, nullptr, FALSE); }
static void BwUpdateButtons() {
    bool any = !g_bw.empty(), run = g_bwRunning;
    bool merge = CbSel(g_bwMode) == 1;
    for (int id : {ID_BW_ADD, ID_BW_UP, ID_BW_DOWN, ID_BW_REMOVE, ID_BW_CLEAR, ID_BW_SORT}) EnableWindow(GetDlgItem(g_bwWnd, id), !run);
    EnableWindow(g_bwMode, !run);
    EnableWindow(g_bwDest, !run);
    ShowWindow(g_bwDest, merge ? SW_HIDE : SW_SHOW);
    HWND st = GetDlgItem(g_bwWnd, ID_BW_START);
    SetWindowTextW(st, run ? L"중지" : merge ? L"합쳐서 열기" : L"정리해서 저장");
    EnableWindow(st, run || any);
    SetWindowTextW(GetDlgItem(g_bwWnd, ID_BW_CLOSE), L"닫기");
    EnableWindow(GetDlgItem(g_bwWnd, ID_BW_CLOSE), !run);
    ShowWindow(GetDlgItem(g_bwWnd, ID_BW_OPENDIR), (!run && !g_bwLastDir.empty()) ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_bwWnd, nullptr, FALSE);
}
static void BwAddFiles(const std::vector<wstring>& files) {
    int added = 0;
    for (auto& f : files) {
        bool dup = false;
        for (auto& b : g_bw) if (_wcsicmp(b.path.c_str(), f.c_str()) == 0) { dup = true; break; }
        if (dup) continue;
        BwItem it; it.path = f;
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &fa)) it.size = ((long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
        wstring why = CheckTextFile(f, kMaxBatchBytes);
        it.status = why.empty() ? L"대기" : L"지원 안 함: " + why;
        g_bw.push_back(it);
        added++;
    }
    BwRefresh();
    BwSetMsg(std::to_wstring(g_bw.size()) + L"개 파일. 순서는 [위로]·[아래로]로 바꿀 수 있습니다.");
    BwUpdateButtons();
}
static std::vector<wstring> PickFiles(HWND owner, const wchar_t* title) {
    std::vector<wchar_t> buf(1 << 17, 0);
    OPENFILENAMEW of = {};
    of.lStructSize = sizeof(of);
    of.hwndOwner = owner;
    of.lpstrFilter = kOpenFilter;
    of.lpstrFile = buf.data();
    of.nMaxFile = (DWORD)buf.size();
    of.lpstrTitle = title;
    of.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    std::vector<wstring> files;
    if (!GetOpenFileNameW(&of)) return files;
    wstring first = buf.data();
    const wchar_t* p = buf.data() + first.size() + 1;
    if (!*p) files.push_back(first);
    else {
        wstring dir = first;
        if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
        while (*p) { files.push_back(dir + p); p += wcslen(p) + 1; }
        // 대화상자가 돌려주는 순서는 제각각이므로 이름순(1권, 2권, 10권 순서)으로
        std::sort(files.begin(), files.end(), [](const wstring& a, const wstring& b) { return StrCmpLogicalW(FileName(a).c_str(), FileName(b).c_str()) < 0; });
    }
    return files;
}

struct BwJob {
    std::vector<wstring> files;
    std::vector<wstring> outs;
    std::vector<char> skip;
    TidyOptions o;
};
struct BwStep { int idx; wstring status; };
static DWORD WINAPI BwThread(LPVOID p) {
    BwJob* j = (BwJob*)p;
    for (size_t i = 0; i < j->files.size(); ++i) {
        if (g_bwCancel) break;
        const wstring& f = j->files[i];
        wstring st;
        PostMessageW(g_bwWnd, WM_BW_STEP, 0, (LPARAM) new BwStep{(int)i, L"정리하는 중…"});
        if (j->skip[i]) st = L"건너뜀 (이미 있음)";
        else {
            wstring why = CheckTextFile(f, kMaxBatchBytes);
            std::string bytes;
            int enc = 0;
            U32 t;
            if (!why.empty()) st = L"지원 안 함: " + why;
            else if (!ReadAll(f, bytes)) st = L"실패: 읽을 수 없음";
            else if (IsEpubPath(f)) {  // EPUB: 본문 글을 꺼내 정리
                wstring w;
                if (!EpubExtract(bytes, t, w, DecodeEpubDoc)) st = L"지원 안 함: " + w;
                else enc = E_UTF8BOM;
                std::string().swap(bytes);
            } else {
                t = Decode(bytes, enc);
                std::string().swap(bytes);
                if (LooksBinary(t)) st = L"지원 안 함: 텍스트 파일이 아님";
            }
            if (st.empty()) {
                U32 r = TidyProcess(t, j->o, nullptr);
                U32().swap(t);
                wstring rw = U32ToW(r, true);
                U32().swap(r);
                int senc = SaveEncoding(enc);
                if (EncCp(senc) && HasInvisible(rw)) senc = E_UTF8BOM;
                int lossy = 0;
                std::string ob = Encode(rw, senc, &lossy);
                if (!WriteAll(j->outs[i], ob)) st = L"실패: 저장할 수 없음";
                else st = lossy ? L"완료 ('?'로 바뀐 글자 " + std::to_wstring(lossy) + L"개)" : L"완료";
            }
        }
        PostMessageW(g_bwWnd, WM_BW_STEP, 1, (LPARAM) new BwStep{(int)i, st});
    }
    PostMessageW(g_bwWnd, WM_BW_DONE, 0, (LPARAM)j);
    return 0;
}
static void BwStart() {
    if (g_bw.empty()) return;
    if (CbSel(g_bwMode) == 1) {  // 합쳐서 열기
        std::vector<wstring> files;
        for (auto& b : g_bw) files.push_back(b.path);
        if (!ConfirmDiscardEdits()) return;
        if (LoadMerged(files, g_bwWnd)) { AfterTextChange(); DestroyWindow(g_bwWnd); }
        return;
    }
    if (CbSel(g_bwDest) == 1 && g_bwDir.empty()) { SendMessageW(g_bwWnd, WM_COMMAND, MAKEWPARAM(ID_BW_DEST, CBN_SELCHANGE), (LPARAM)g_bwDest); if (g_bwDir.empty()) return; }
    BwJob* j = new BwJob;
    int exist = 0;
    for (auto& b : g_bw) { j->files.push_back(b.path); j->outs.push_back(BwOutPath(b.path)); }
    for (auto& o : j->outs) if (FileExists(o)) exist++;
    bool overwrite = true;
    if (exist) {
        wstring m = L"정리된 파일(_정리)이 이미 " + std::to_wstring(exist) + L"개 있습니다.\n\n예: 덮어쓰기\n아니요: 이미 있는 파일은 건너뛰기";
        int r = MessageBoxW(g_bwWnd, m.c_str(), L"여러 파일 정리", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) { delete j; return; }
        overwrite = r == IDYES;
    }
    for (auto& o : j->outs) j->skip.push_back(!overwrite && FileExists(o));
    j->o = ReadOptions();
    for (auto& b : g_bw) if (b.status.compare(0, 5, L"지원 안 함") != 0) b.status = L"대기";
    BwRefresh();
    g_bwLastDir = CbSel(g_bwDest) == 1 ? g_bwDir : DirOf(g_bw[0].path);
    g_bwCancel = 0;
    g_bwRunning = true;
    SendMessageW(g_bwProg, PBM_SETRANGE32, 0, (LPARAM)g_bw.size());
    SendMessageW(g_bwProg, PBM_SETPOS, 0, 0);
    BwSetMsg(L"정리하는 중…");
    BwUpdateButtons();
    HANDLE th = CreateThread(nullptr, 8 << 20, BwThread, j, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (th) CloseHandle(th); else BwThread(j);
}
static RECT g_bwListFrame;  // 목록 둥근 테두리
static LRESULT CALLBACK BwListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_PAINT && SendMessageW(h, LVM_GETITEMCOUNT, 0, 0) == 0) {  // 빈 목록: 가운데 안내
        LRESULT r = DefSubclassProc(h, msg, wp, lp);
        HDC dc = GetDC(h);
        RECT rc; GetClientRect(h, &rc);
        RECT hr = {};
        HWND hd = (HWND)SendMessageW(h, LVM_GETHEADER, 0, 0);
        if (hd) GetWindowRect(hd, &hr);
        rc.top += hr.bottom - hr.top;
        DrawTxt(dc, L"정리할 파일을 여기에 끌어다 놓으세요  (여러 개를 한꺼번에 놓아도 됩니다)", rc, g_uiFont, col::faint,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        ReleaseDC(h, dc);
        return r;
    }
    if (msg == WM_SIZE) InvalidateRect(h, nullptr, TRUE);  // 안내 글이 가운데에 남게
    return DefSubclassProc(h, msg, wp, lp);
}
static void BwLayout(HWND h) {
    RECT rc; GetClientRect(h, &rc);
    int W = rc.right, H = rc.bottom;
    int bw = S(132), bh = S(30), gap = S(6), pad = S(16);
    int listR = W - pad - bw - S(14);
    int top = S(44), bottomArea = S(196);
    g_bwListFrame = {pad, top, listR, H - bottomArea};
    MoveWindow(g_bwList, pad + 1, top + 1, listR - pad - 2, H - top - bottomArea - 2, TRUE);
    {   // 마지막 칸(폴더)을 남은 폭에 맞춰 가로 스크롤 막대가 생기지 않게
        RECT lr; GetClientRect(g_bwList, &lr);
        int used = 0;
        for (int i = 0; i < 4; ++i) used += (int)SendMessageW(g_bwList, LVM_GETCOLUMNWIDTH, i, 0);
        int rest = lr.right - used;
        SendMessageW(g_bwList, LVM_SETCOLUMNWIDTH, 4, std::max<int>(S(80), rest));
    }
    int y = top;
    for (int id : {ID_BW_ADD, ID_BW_UP, ID_BW_DOWN, ID_BW_SORT, ID_BW_REMOVE, ID_BW_CLEAR}) {  // [추가] / [위로·아래로·정렬] / [빼기·모두 빼기]
        MoveWindow(GetDlgItem(h, id), W - pad - bw, y, bw, bh, TRUE);
        y += bh + gap;
        if (id == ID_BW_ADD || id == ID_BW_SORT) y += S(12);
    }
    int oy = H - bottomArea + S(14);
    int lw = S(80), cw = S(300);
    MoveWindow(g_bwMode, pad + lw, oy, cw, S(300), TRUE);
    MoveWindow(g_bwDest, pad + lw, oy + S(36), cw, S(300), TRUE);
    MoveWindow(g_bwProg, pad, H - S(80), W - pad * 2, S(6), TRUE);
    int by = H - S(48);
    MoveWindow(GetDlgItem(h, ID_BW_CLOSE), W - pad - S(90), by, S(90), S(34), TRUE);
    MoveWindow(GetDlgItem(h, ID_BW_START), W - pad - S(90) - S(8) - S(150), by, S(150), S(34), TRUE);
    MoveWindow(GetDlgItem(h, ID_BW_OPENDIR), pad, by, S(140), S(34), TRUE);
}
static LRESULT CALLBACK BwProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_bwWnd = h;
        auto btn = [&](int id, const wchar_t* t) {
            HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SetWindowSubclass(b, BtnProc, 0, 0);
            return b;
        };
        g_bwList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
                                   0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_BW_LIST, g_hInst, nullptr);  // 테두리는 창에서 둥글게 그림
        SetWindowSubclass(g_bwList, BwListProc, 0, 0);
        SendMessageW(g_bwList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessageW(g_bwList, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        ListView_SetBkColor(g_bwList, col::field); ListView_SetTextBkColor(g_bwList, col::field); ListView_SetTextColor(g_bwList, col::text);
        ApplyScrollTheme(g_bwList);
        ApplyCtlTheme((HWND)SendMessageW(g_bwList, LVM_GETHEADER, 0, 0), L"DarkMode_ItemsView");  // 목록 머리줄도 어둡게
        struct { const wchar_t* t; int w; int fmt; } cols[] = {{L"순서", 44, LVCFMT_RIGHT}, {L"파일 이름", 250, LVCFMT_LEFT}, {L"크기", 76, LVCFMT_RIGHT},
                                                               {L"상태", 150, LVCFMT_LEFT}, {L"폴더", 200, LVCFMT_LEFT}};
        for (int i = 0; i < 5; ++i) {
            LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; c.pszText = (LPWSTR)cols[i].t; c.cx = S(cols[i].w); c.fmt = cols[i].fmt;
            SendMessageW(g_bwList, LVM_INSERTCOLUMNW, i, (LPARAM)&c);
        }
        btn(ID_BW_ADD, L"+  파일 추가…"); btn(ID_BW_UP, L"▲ 위로"); btn(ID_BW_DOWN, L"▼ 아래로");
        btn(ID_BW_SORT, L"이름순 정렬"); btn(ID_BW_REMOVE, L"목록에서 빼기"); btn(ID_BW_CLEAR, L"모두 빼기");
        btn(ID_BW_START, L"정리해서 저장"); btn(ID_BW_CLOSE, L"닫기"); btn(ID_BW_OPENDIR, L"저장한 폴더 열기");
        auto combo = [&](int id, std::initializer_list<const wchar_t*> items) {
            HWND c = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                                     0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            for (auto s : items) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
            SendMessageW(c, CB_SETCURSEL, 0, 0);
            SetWindowSubclass(c, ComboProc, 0, 1);  // 주 창과 같은 모양 (모서리 바깥은 이 창의 바탕색)
            return c;
        };
        g_bwMode = combo(ID_BW_MODE, {L"파일마다 따로 정리해서 저장", L"순서대로 하나로 합쳐서 열기"});
        g_bwDest = combo(ID_BW_DEST, {L"원본과 같은 폴더에 저장", L"다른 폴더에 저장…"});
        g_bwProg = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_BW_PROG, g_hInst, nullptr);
        DragAcceptFiles(h, TRUE);
        return 0;
    }
    case WM_SIZE: BwLayout(h); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO* mm = (MINMAXINFO*)lp; mm->ptMinTrackSize.x = S(640); mm->ptMinTrackSize.y = S(420); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC dc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ ob = SelectObject(dc, bmp);
        FillRect2(dc, rc, col::panel);
        int pad = S(16), H = rc.bottom;
        RECT t = {pad, S(12), rc.right - pad, S(38)};
        DrawTxt(dc, L"파일을 이 창에 끌어다 놓거나 [파일 추가…]를 누르세요. 합칠 때는 목록 순서대로 이어 붙입니다.", t, g_uiFont, col::sub);
        int oy = H - S(196) + S(14);
        RECT l1 = {pad, oy, pad + S(80), oy + S(26)}, l2 = {pad, oy + S(36), pad + S(80), oy + S(62)};
        DrawTxt(dc, L"작업", l1, g_uiFont, col::label);
        DrawTxt(dc, CbSel(g_bwMode) == 1 ? L"" : L"저장 위치", l2, g_uiFont, col::label);
        if (CbSel(g_bwMode) == 0) {
            RECT d = {pad + S(80) + S(312), oy + S(36), rc.right - pad, oy + S(62)};
            wstring dm = CbSel(g_bwDest) == 1 ? g_bwDir : L"이름은 '파일명_정리.txt'";
            DrawTxt(dc, dm, d, g_smallFont, col::faint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
        } else {
            RECT d = {pad + S(80), oy + S(36), rc.right - pad, oy + S(62)};
            DrawTxt(dc, L"합친 글을 화면에 열어 확인한 뒤 [저장]하면 '첫 파일명_합본_정리.txt'로 저장됩니다.", d, g_smallFont, col::faint);
        }
        RECT m = {pad, H - S(108), rc.right - pad, H - S(84)};
        DrawTxt(dc, g_bwMsg, m, g_uiFont, col::text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        {   // 목록 둥근 테두리 + 아래 단추 줄 구분선
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            FillRound(g, g_bwListFrame, S(6), col::field, col::border);
        }
        RECT fl = {0, H - S(62), rc.right, H - S(61)};
        FillRect2(dc, fl, col::panelLine);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        SetBkColor((HDC)wp, col::panel);
        SetTextColor((HDC)wp, col::text);
        return (LRESULT)g_brPanel;
    }
    case WM_CTLCOLORLISTBOX: {  // 펼친 선택목록 바탕 (어둡게 테마)
        SetBkColor((HDC)wp, col::field);
        SetTextColor((HDC)wp, col::text);
        return (LRESULT)g_brField;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* m = (MEASUREITEMSTRUCT*)lp;
        if (m->CtlType == ODT_COMBOBOX) { m->itemHeight = (m->itemID == (UINT)-1) ? S(20) : S(24); return TRUE; }  // 닫힌 칸 20 + 테두리 = 26
        break;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* d = (const DRAWITEMSTRUCT*)lp;
        if (d->CtlType == ODT_COMBOBOX) DrawComboItem(d); else DrawButton(d);
        return TRUE;
    }
    case WM_DROPFILES: {
        HDROP hd = (HDROP)wp;
        std::vector<wstring> files;
        UINT cnt = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < cnt; ++i) {
            UINT len = DragQueryFileW(hd, i, nullptr, 0);
            wstring f(len + 1, 0); DragQueryFileW(hd, i, &f[0], len + 1); f.resize(len);
            if (FileExists(f)) files.push_back(f);
        }
        DragFinish(hd);
        std::sort(files.begin(), files.end(), [](const wstring& a, const wstring& b) { return StrCmpLogicalW(FileName(a).c_str(), FileName(b).c_str()) < 0; });
        if (!g_bwRunning) BwAddFiles(files);
        return 0;
    }
    case WM_BW_STEP: {
        BwStep* st = (BwStep*)lp;
        if (st->idx >= 0 && st->idx < (int)g_bw.size()) {
            g_bw[st->idx].status = st->status;
            wstring t = st->status;
            LVITEMW s2 = {}; s2.iSubItem = 3; s2.pszText = (LPWSTR)t.c_str();
            SendMessageW(g_bwList, LVM_SETITEMTEXTW, st->idx, (LPARAM)&s2);
            SendMessageW(g_bwList, LVM_ENSUREVISIBLE, st->idx, FALSE);
            if (wp == 1) SendMessageW(g_bwProg, PBM_SETPOS, st->idx + 1, 0);
            else BwSetMsg(L"정리하는 중 (" + std::to_wstring(st->idx + 1) + L"/" + std::to_wstring(g_bw.size()) + L"): " + FileName(g_bw[st->idx].path));
        }
        delete st;
        return 0;
    }
    case WM_BW_DONE: {
        delete (BwJob*)lp;
        g_bwRunning = false;
        int ok = 0, bad = 0, skip = 0;
        for (auto& b : g_bw) {
            if (b.status.compare(0, 2, L"완료") == 0) ok++;
            else if (b.status.compare(0, 3, L"건너뜀") == 0) skip++;
            else if (b.status != L"대기") bad++;
        }
        wstring m = (g_bwCancel ? L"중지했습니다. " : L"끝났습니다. ") + std::to_wstring(ok) + L"개 저장";
        if (skip) m += L", " + std::to_wstring(skip) + L"개 건너뜀";
        if (bad) m += L", " + std::to_wstring(bad) + L"개 못 함 (상태 칸 참고)";
        m += L"  ·  저장 폴더: " + g_bwLastDir;
        BwSetMsg(m);
        BwUpdateButtons();
        SetStatus(L"여러 파일 정리: " + std::to_wstring(ok) + L"개 저장");
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)lp;
        if (nh->idFrom == ID_BW_LIST && nh->code == LVN_KEYDOWN) {
            NMLVKEYDOWN* k = (NMLVKEYDOWN*)lp;
            if (k->wVKey == VK_DELETE && !g_bwRunning) SendMessageW(h, WM_COMMAND, ID_BW_REMOVE, 0);
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        switch (id) {
        case ID_BW_ADD: BwAddFiles(PickFiles(h, L"추가할 파일을 고르세요 (여러 개 선택 가능)")); return 0;
        case ID_BW_UP: case ID_BW_DOWN: {
            std::vector<int> sel = BwSelected();
            if (sel.empty()) return 0;
            std::vector<char> on(g_bw.size(), 0);
            for (int i : sel) on[i] = 1;
            if (id == ID_BW_UP) {
                for (size_t i = 1; i < g_bw.size(); ++i)
                    if (on[i] && !on[i - 1]) { std::swap(g_bw[i], g_bw[i - 1]); std::swap(on[i], on[i - 1]); }
            } else {
                for (size_t i = g_bw.size() - 1; i-- > 0;)
                    if (on[i] && !on[i + 1]) { std::swap(g_bw[i], g_bw[i + 1]); std::swap(on[i], on[i + 1]); }
            }
            std::vector<int> ns;
            for (size_t i = 0; i < on.size(); ++i) if (on[i]) ns.push_back((int)i);
            BwRefresh(ns);
            SetFocus(g_bwList);
            return 0;
        }
        case ID_BW_REMOVE: {
            std::vector<int> sel = BwSelected();
            for (size_t k = sel.size(); k-- > 0;) g_bw.erase(g_bw.begin() + sel[k]);
            int next = sel.empty() ? -1 : std::min(sel.front(), (int)g_bw.size() - 1);
            BwRefresh(next >= 0 ? std::vector<int>{next} : std::vector<int>{});
            BwSetMsg(std::to_wstring(g_bw.size()) + L"개 파일");
            BwUpdateButtons();
            return 0;
        }
        case ID_BW_CLEAR: g_bw.clear(); BwRefresh(); BwSetMsg(L""); BwUpdateButtons(); return 0;
        case ID_BW_SORT:
            std::stable_sort(g_bw.begin(), g_bw.end(), [](const BwItem& a, const BwItem& b) { return StrCmpLogicalW(FileName(a.path).c_str(), FileName(b.path).c_str()) < 0; });
            BwRefresh();
            return 0;
        case ID_BW_MODE:
            if (code == CBN_SELCHANGE) BwUpdateButtons();
            return 0;
        case ID_BW_DEST:
            if ((code == CBN_SELENDOK || code == CBN_SELCHANGE) && CbSel(g_bwDest) == 1 && !g_bwPicking) {
                g_bwPicking = true;
                BROWSEINFOW bi = {};
                bi.hwndOwner = h;
                bi.lpszTitle = L"정리한 파일을 저장할 폴더를 고르세요";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pl = SHBrowseForFolderW(&bi);
                wchar_t path[MAX_PATH * 2] = {};
                if (pl && SHGetPathFromIDListW(pl, path)) g_bwDir = path;
                if (pl) CoTaskMemFree(pl);
                if (g_bwDir.empty()) SendMessageW(g_bwDest, CB_SETCURSEL, 0, 0);
                g_bwPicking = false;
            }
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case ID_BW_START:
            if (g_bwRunning) { InterlockedExchange(&g_bwCancel, 1); BwSetMsg(L"지금 파일까지만 하고 멈춥니다…"); }
            else BwStart();
            return 0;
        case ID_BW_OPENDIR:
            if (!g_bwLastDir.empty()) ShellExecuteW(h, L"open", g_bwLastDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case ID_BW_CLOSE: case IDCANCEL:
            if (!g_bwRunning) DestroyWindow(h);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (!g_bwRunning) DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        EnableWindow(g_hwnd, TRUE);
        SetForegroundWindow(g_hwnd);
        RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        g_bwWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static void OpenBatchWindow(const std::vector<wstring>& files) {
    if (g_bwWnd) { BwAddFiles(files); SetForegroundWindow(g_bwWnd); return; }
    static bool reg = false;
    if (!reg) {
        INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&icc);
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = BwProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"TextTidyBatch";
        wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        reg = true;
    }
    g_bw.clear();
    g_bwLastDir.clear();
    g_bwMsg.clear();
    RECT mr; GetWindowRect(g_hwnd, &mr);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int w = std::min<int>(S(820), wa.right - wa.left), hh = std::min<int>(S(560), wa.bottom - wa.top);
    int x = (mr.left + mr.right - w) / 2, y = (mr.top + mr.bottom - hh) / 2;
    x = std::max<int>(wa.left, std::min<int>(x, wa.right - w)); y = std::max<int>(wa.top, std::min<int>(y, wa.bottom - hh));
    HWND bw = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TextTidyBatch", L"여러 파일 정리·합치기", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                              x, y, w, hh, g_hwnd, nullptr, g_hInst, nullptr);
    if (!bw) return;
    ApplyTitleBar(bw);
    EnableWindow(g_hwnd, FALSE);
    BwAddFiles(files);
    if (files.empty()) BwSetMsg(L"파일을 추가하세요.");
    BwUpdateButtons();
    ShowWindow(bw, SW_SHOW);
}
static void DoBatchDialog() { OpenBatchWindow({}); }

static void HandleDrop(HDROP hd) {
    UINT cnt = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
    std::vector<wstring> files;
    for (UINT i = 0; i < cnt; ++i) {
        UINT len = DragQueryFileW(hd, i, nullptr, 0);
        wstring s(len + 1, 0);
        DragQueryFileW(hd, i, &s[0], len + 1);
        s.resize(len);
        DWORD a = GetFileAttributesW(s.c_str());
        if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(s);
    }
    std::sort(files.begin(), files.end(), [](const wstring& a, const wstring& b) { return StrCmpLogicalW(FileName(a).c_str(), FileName(b).c_str()) < 0; });
    if (files.size() == 1) LoadFile(files[0]);
    else if (files.size() > 1) OpenBatchWindow(files);
}

static void SaveSettings() {
    IniSet(L"blank", CbSel(g_cbBlank));
    IniSet(L"join", CbSel(g_cbJoin));
    IniSet(L"space", CbSel(g_cbSpace));
    IniSet(L"optver", 2);
    IniSet(L"split", SplitMode());
    IniSet(L"longlen", LongLenValue());
    IniSet(L"sync", Checked(g_chkSync));
    IniSet(L"parasep", CbSel(g_cbParaSep));
    IniSet(L"scene", CbSel(g_cbScene));
    IniSet(L"indent", CbSel(g_cbIndent));
    IniSet(L"quote", CbSel(g_cbQuote));
    IniSet(L"ellipsis", CbSel(g_cbEll));
    IniSet(L"collapse", Checked(g_chkCollapse));
    IniSet(L"junk", Checked(g_chkJunk));
    IniSet(L"hanja", Checked(g_chkHanja));
    IniSet(L"half", Checked(g_chkHalf));
    IniSet(L"readenc", CbSel(g_cbReadEnc));
    IniSet(L"enc", CbSel(g_cbEnc));
    IniSet(L"fold", g_fold);
    WINDOWPLACEMENT wp = {sizeof(wp)};
    if (GetWindowPlacement(g_hwnd, &wp)) {
        IniSet(L"maximized", wp.showCmd == SW_SHOWMAXIMIZED);
        IniSet(L"w", MulDiv(wp.rcNormalPosition.right - wp.rcNormalPosition.left, 96, g_dpi));
        IniSet(L"h", MulDiv(wp.rcNormalPosition.bottom - wp.rcNormalPosition.top, 96, g_dpi));
    }
}

static void AfterTextChange() {
    bool has = !g_text.empty() && !g_busy;
    EnableWindow(g_btnSave, has);
    EnableWindow(g_btnSaveAs, has);
    InvalidateRect(g_hwnd, &g_rcTool, FALSE);
}

// 모든 옵션을 처음 상태로 (초기값은 CreateUI의 IniGet 기본값과 같게 유지)
static void ResetOptions() {
    auto sel = [](HWND h, int i) { SendMessageW(h, CB_SETCURSEL, i, 0); };
    sel(g_cbBlank, 0); sel(g_cbJoin, 0); sel(g_cbSpace, 0);
    sel(g_cbSplit, 0); sel(g_cbScene, 0); sel(g_cbParaSep, 0);
    sel(g_cbIndent, 0); sel(g_cbQuote, 0); sel(g_cbEll, 0);
    sel(g_cbEnc, 0);
    SetToggle(g_chkCollapse, true);
    SetToggle(g_chkJunk, false);
    SetToggle(g_chkHanja, false);
    SetToggle(g_chkHalf, false);
    SetToggle(g_chkSync, true);
    g_loading = true;
    SetWindowTextW(g_edLong, L"400");
    EnableWindow(g_edLong, FALSE);
    g_loading = false;
    KillTimer(g_hwnd, TIMER_LONG);
    bool encChanged = CbSel(g_cbReadEnc) != 0;
    sel(g_cbReadEnc, 0);
    g_forceEnc = 0;
    if (encChanged && !g_path.empty()) Reload();  // 읽기 인코딩을 되돌렸으면 다시 읽음
    else if (!g_text.empty()) Reprocess();
    SetStatus(L"옵션을 처음 상태로 되돌렸습니다.");
}

static void OptionChanged() {
    if (g_text.empty()) return;
    Reprocess();
}

// ---------------------------------------------------------------------------
// 바꾸기·지우기 목록 창 (규칙 = 정리할 때마다 자동으로 적용)
static HWND g_rpWnd;  // 찾기·바꾸기 창
static HWND g_rulesWnd, g_rlList, g_rlFind, g_rlRepl, g_rlRegex, g_rlLine, g_rlInfo;
static std::vector<RuleItem> g_rlItems;
static std::vector<wstring> g_rlComments;  // 목록 파일의 설명 줄 (저장할 때 그대로 남김)
static bool g_rlFilling = false;
static const wchar_t* RuleKindName(const RuleItem& r) {
    switch (r.kind) {
    case 1: return L"줄 지우기";
    case 2: return r.t.empty() ? L"지우기 (정규식)" : L"바꾸기 (정규식)";
    case 3: return L"줄 지우기 (정규식)";
    default: return r.t.empty() ? L"지우기" : L"바꾸기";
    }
}
static void RlFill(int sel) {
    g_rlFilling = true;
    SendMessageW(g_rlList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_rlList);
    for (size_t i = 0; i < g_rlItems.size(); ++i) {
        const RuleItem& r = g_rlItems[i];
        LVITEMW it = {};
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.pszText = (LPWSTR)RuleKindName(r);
        int at = (int)SendMessageW(g_rlList, LVM_INSERTITEMW, 0, (LPARAM)&it);
        wstring t = (r.kind == 1 || r.kind == 3) ? L"(줄 전체)" : r.t.empty() ? L"(지움)" : r.t;
        ListView_SetItemText(g_rlList, at, 1, (LPWSTR)r.f.c_str());
        ListView_SetItemText(g_rlList, at, 2, (LPWSTR)t.c_str());
        ListView_SetCheckState(g_rlList, at, r.on);
    }
    if (sel >= 0 && sel < (int)g_rlItems.size()) {
        ListView_SetItemState(g_rlList, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(g_rlList, sel, FALSE);
    }
    SendMessageW(g_rlList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_rlList, nullptr, TRUE);
    g_rlFilling = false;
}
static int RlSel() { return (int)SendMessageW(g_rlList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); }
static bool RlFormRule(HWND owner, RuleItem& r) {
    wchar_t buf[2048];
    GetWindowTextW(g_rlFind, buf, 2048); r.f = buf;
    GetWindowTextW(g_rlRepl, buf, 2048); r.t = buf;
    bool rx = Checked(g_rlRegex), line = Checked(g_rlLine);
    r.kind = line ? (rx ? 3 : 1) : (rx ? 2 : 0);
    if (line) r.t.clear();
    while (!r.f.empty() && r.f.back() == L' ' && !rx) r.f.pop_back();
    if (r.f.empty()) { MessageBoxW(owner, L"찾을 말을 적어 주세요.", L"바꾸기·지우기 목록", MB_ICONINFORMATION); SetFocus(g_rlFind); return false; }
    if (!RuleStorable(r)) {
        MessageBoxW(owner, L"이 말은 규칙으로 저장할 수 없습니다.\n찾을 말·바꿀 말의 앞뒤 공백, #로 시작하는 말, => 가 든 말은 쓸 수 없습니다.", L"바꾸기·지우기 목록", MB_ICONINFORMATION);
        return false;
    }
    if (rx) {
        try { std::wregex re(r.f, std::regex::ECMAScript); }
        catch (...) { MessageBoxW(owner, L"정규식 문법이 맞지 않습니다. 다시 확인해 주세요.", L"바꾸기·지우기 목록", MB_ICONWARNING); SetFocus(g_rlFind); return false; }
    }
    r.on = true;
    return true;
}
static void RlFormFrom(const RuleItem& r) {
    SetWindowTextW(g_rlFind, r.f.c_str());
    SetWindowTextW(g_rlRepl, r.t.c_str());
    SetToggle(g_rlRegex, r.kind == 2 || r.kind == 3);
    SetToggle(g_rlLine, r.kind == 1 || r.kind == 3);
    EnableWindow(g_rlRepl, !(r.kind == 1 || r.kind == 3));
}
static void RlReadChecks() {
    for (size_t i = 0; i < g_rlItems.size(); ++i) g_rlItems[i].on = ListView_GetCheckState(g_rlList, (int)i) != 0;
}
static bool SaveRuleItems(const std::vector<RuleItem>& v, const std::vector<wstring>& comments, HWND owner) {
    wstring w = L"# 야문 클린리더 바꾸기·지우기 목록 (프로그램의 [바꾸기·지우기 목록]에서 고칠 수 있습니다)\r\n";
    for (auto& c : comments) w += c + L"\r\n";
    for (auto& r : v) w += RuleLine(r) + L"\r\n";
    std::string bytes = Encode(w, E_UTF8BOM, nullptr);
    if (!WriteAll(g_rulesPath, bytes)) {
        MessageBoxW(owner, (L"목록을 저장하지 못했습니다:\n" + g_rulesPath).c_str(), L"야문 클린리더", MB_ICONERROR);
        return false;
    }
    ParseRules(w);
    return true;
}
static void RlPresetMenu(HWND h, HWND anchor) {
    struct P { const wchar_t* name; int kind; const wchar_t* f; const wchar_t* t; };
    static const P ps[] = {
        {L"느낌표·물음표 여러 개를 하나로   (!!!! → !)", 2, L"([!?])\\1+", L"$1"},
        {L"ㅋㅋㅋ 길게 쓴 것 줄이기   (ㅋㅋㅋㅋㅋ → ㅋㅋ)", 2, L"ㅋ{3,}", L"ㅋㅋ"},
        {L"ㅎㅎㅎ 길게 쓴 것 줄이기   (ㅎㅎㅎㅎ → ㅎㅎ)", 2, L"ㅎ{3,}", L"ㅎㅎ"},
        {L"물결표 여러 개 줄이기   (~~~~ → ~~)", 2, L"~{3,}", L"~~"},
        {L"쪽 번호 줄 지우기   (- 12 -)", 3, L"^\\s*-\\s*\\d+\\s*-\\s*$", L""},
        {L"숫자만 있는 회차 줄을 제목으로   (12 화 → 제12화)", 2, L"^\\s*(\\d+)\\s*화\\s*$", L"제$1화"},
    };
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < (int)(sizeof(ps) / sizeof(ps[0])); ++i) AppendMenuW(m, MF_STRING, 1 + i, ps[i].name);
    RECT r; GetWindowRect(anchor, &r);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, h, nullptr);
    DestroyMenu(m);
    if (cmd < 1) return;
    const P& p = ps[cmd - 1];
    RuleItem it; it.kind = p.kind; it.f = p.f; it.t = p.t; it.on = true;
    RlReadChecks();
    for (auto& x : g_rlItems) if (x.kind == it.kind && x.f == it.f && x.t == it.t) { SetWindowTextW(g_rlInfo, L"이미 목록에 있는 규칙입니다."); return; }
    g_rlItems.push_back(it);
    RlFill((int)g_rlItems.size() - 1);
    SetWindowTextW(g_rlInfo, L"규칙을 추가했습니다. [저장하고 적용]을 누르면 쓰입니다.");
}
static LRESULT CALLBACK RulesProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        auto btn = [&](int id, const wchar_t* t) {
            HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SetWindowSubclass(b, BtnProc, 0, 0);
            return b;
        };
        auto lab = [&](int id, const wchar_t* t, HFONT f) {
            HWND s = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(s, WM_SETFONT, (WPARAM)f, TRUE);
            return s;
        };
        auto edit = [&](int id) {
            HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(e, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            return e;
        };
        lab(ID_RL_L1, L"찾을 말", g_uiFont);
        g_rlFind = edit(ID_RL_FIND);
        lab(ID_RL_L2, L"바꿀 말", g_uiFont);
        g_rlRepl = edit(ID_RL_REPL);
        g_rlRegex = btn(ID_RL_REGEX, L"정규식");
        g_rlLine = btn(ID_RL_LINE, L"이 말이 든 줄을 통째로 지우기");
        btn(ID_RL_ADD, L"목록에 추가");
        btn(ID_RL_EDIT, L"고른 규칙 고치기");
        btn(ID_RL_PRESET, L"자주 쓰는 규칙 ▼");
        g_rlList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                                   0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_RL_LIST, g_hInst, nullptr);
        SendMessageW(g_rlList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
        SendMessageW(g_rlList, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        ListView_SetBkColor(g_rlList, col::field); ListView_SetTextBkColor(g_rlList, col::field); ListView_SetTextColor(g_rlList, col::text);
        struct { const wchar_t* t; int w; } cols[] = {{L"하는 일", 150}, {L"찾을 말", 260}, {L"바꿀 말", 240}};
        for (int i = 0; i < 3; ++i) {
            LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)cols[i].t; c.cx = S(cols[i].w);
            SendMessageW(g_rlList, LVM_INSERTCOLUMNW, i, (LPARAM)&c);
        }
        btn(ID_RL_UP, L"▲ 위로"); btn(ID_RL_DOWN, L"▼ 아래로"); btn(ID_RL_DEL, L"목록에서 빼기");
        btn(ID_RL_OK, L"저장하고 적용"); btn(ID_RL_CANCEL, L"취소");
        g_rlInfo = lab(ID_RL_INFO, L"", g_smallFont);
        lab(ID_RL_HELP, L"규칙은 정리할 때마다 위에서부터 차례로 자동 적용됩니다 (다른 파일에도).  체크를 끄면 쓰지 않고 남겨 둡니다.  바꿀 말을 비우면 찾은 말을 지웁니다.", g_smallFont);
        g_rlComments.clear();
        g_rlItems = RulesFromText(LoadRulesText(), &g_rlComments);
        RlFill(-1);
        return 0;
    }
    case WM_SIZE: {
        int W = LOWORD(lp), H = HIWORD(lp), p = S(14), lw = S(56), rh = S(28);
        int fw = (W - p * 2 - lw * 2 - S(16)) / 2;
        MoveWindow(GetDlgItem(h, ID_RL_L1), p, S(12), lw, rh, TRUE);
        MoveWindow(g_rlFind, p + lw, S(12), fw, rh, TRUE);
        MoveWindow(GetDlgItem(h, ID_RL_L2), p + lw + fw + S(16), S(12), lw, rh, TRUE);
        MoveWindow(g_rlRepl, p + lw * 2 + fw + S(16), S(12), W - p - (p + lw * 2 + fw + S(16)), rh, TRUE);
        int y = S(48), x = p + lw;
        auto place = [&](int id, int w) { MoveWindow(GetDlgItem(h, id), x, y, w, rh, TRUE); x += w + S(8); };
        place(ID_RL_REGEX, MeasureW(g_uiFont, L"정규식") + S(30));
        place(ID_RL_LINE, MeasureW(g_uiFont, L"이 말이 든 줄을 통째로 지우기") + S(30));
        x += S(8);
        place(ID_RL_ADD, MeasureW(g_btnFont, L"목록에 추가") + S(28));
        place(ID_RL_EDIT, MeasureW(g_btnFont, L"고른 규칙 고치기") + S(28));
        int pw = MeasureW(g_btnFont, L"자주 쓰는 규칙 ▼") + S(28);
        MoveWindow(GetDlgItem(h, ID_RL_PRESET), W - p - pw, y, pw, rh, TRUE);
        int ly = S(90), by = H - S(46);
        MoveWindow(GetDlgItem(h, ID_RL_HELP), p, ly, W - p * 2, S(20), TRUE);
        MoveWindow(g_rlList, p, ly + S(24), W - p * 2, by - S(10) - (ly + S(24)), TRUE);
        x = p;
        y = by;
        place(ID_RL_UP, S(84)); place(ID_RL_DOWN, S(90)); place(ID_RL_DEL, S(116));
        MoveWindow(g_rlInfo, x + S(4), by, W - p - S(244) - (x + S(4)), rh, TRUE);
        MoveWindow(GetDlgItem(h, ID_RL_OK), W - p - S(80) - S(8) - S(140), by, S(140), S(32), TRUE);
        MoveWindow(GetDlgItem(h, ID_RL_CANCEL), W - p - S(80), by, S(80), S(32), TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: { MINMAXINFO* mm = (MINMAXINFO*)lp; mm->ptMinTrackSize.x = S(820); mm->ptMinTrackSize.y = S(420); return 0; }
    case WM_DRAWITEM: DrawButton((const DRAWITEMSTRUCT*)lp); return TRUE;
    case WM_CTLCOLORSTATIC: {
        SetBkColor((HDC)wp, col::panel);
        int id = GetDlgCtrlID((HWND)lp);
        SetTextColor((HDC)wp, id == ID_RL_INFO ? col::okText : id == ID_RL_HELP ? col::sub : col::text);
        return (LRESULT)g_brPanel;
    }
    case WM_CTLCOLOREDIT: {
        SetBkColor((HDC)wp, col::field);
        SetTextColor((HDC)wp, col::text);
        return (LRESULT)g_brField;
    }
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)lp;
        if (nh->idFrom == ID_RL_LIST && nh->code == LVN_ITEMCHANGED && !g_rlFilling) {
            NMLISTVIEW* nv = (NMLISTVIEW*)lp;
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED) && nv->iItem >= 0 && nv->iItem < (int)g_rlItems.size())
                RlFormFrom(g_rlItems[nv->iItem]);
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDCANCEL) { DestroyWindow(h); return 0; }
        if (id == IDOK) id = ID_RL_ADD;  // 입력 칸에서 Enter: 목록에 추가
        if (HIWORD(wp) != BN_CLICKED && id != ID_RL_ADD) break;
        switch (id) {
        case ID_RL_REGEX: SetToggle(g_rlRegex, !Checked(g_rlRegex)); return 0;
        case ID_RL_LINE: SetToggle(g_rlLine, !Checked(g_rlLine)); EnableWindow(g_rlRepl, !Checked(g_rlLine)); return 0;
        case ID_RL_ADD: {
            RuleItem r;
            if (!RlFormRule(h, r)) return 0;
            RlReadChecks();
            g_rlItems.push_back(r);
            RlFill((int)g_rlItems.size() - 1);
            SetWindowTextW(g_rlFind, L""); SetWindowTextW(g_rlRepl, L"");
            SetFocus(g_rlFind);
            SetWindowTextW(g_rlInfo, L"추가했습니다. [저장하고 적용]을 누르면 쓰입니다.");
            return 0;
        }
        case ID_RL_EDIT: {
            int s = RlSel();
            if (s < 0) { SetWindowTextW(g_rlInfo, L"목록에서 고칠 규칙을 먼저 고르세요."); return 0; }
            RuleItem r;
            if (!RlFormRule(h, r)) return 0;
            RlReadChecks();
            r.on = g_rlItems[s].on;
            g_rlItems[s] = r;
            RlFill(s);
            SetWindowTextW(g_rlInfo, L"고쳤습니다.");
            return 0;
        }
        case ID_RL_UP: case ID_RL_DOWN: {
            int s = RlSel();
            int t = s + (id == ID_RL_UP ? -1 : 1);
            if (s < 0 || t < 0 || t >= (int)g_rlItems.size()) return 0;
            RlReadChecks();
            std::swap(g_rlItems[s], g_rlItems[t]);
            RlFill(t);
            return 0;
        }
        case ID_RL_DEL: {
            int s = RlSel();
            if (s < 0) return 0;
            RlReadChecks();
            g_rlItems.erase(g_rlItems.begin() + s);
            RlFill(std::min(s, (int)g_rlItems.size() - 1));
            return 0;
        }
        case ID_RL_PRESET: RlPresetMenu(h, GetDlgItem(h, ID_RL_PRESET)); return 0;
        case ID_RL_OK: {
            RlReadChecks();
            if (g_dstDirty && !g_text.empty() &&
                MessageBoxW(h, L"저장하면 글을 다시 정리합니다.\n결과 칸에서 직접 고친 내용(바꾸기 포함)은 없어집니다. 계속할까요?", L"바꾸기·지우기 목록", MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
                return 0;
            if (!SaveRuleItems(g_rlItems, g_rlComments, h)) return 0;
            if (!g_ruleErrors.empty())
                MessageBoxW(h, (L"다음 정규식은 문법이 맞지 않아 쓰지 않습니다:" + g_ruleErrors).c_str(), L"바꾸기·지우기 목록", MB_ICONWARNING);
            DestroyWindow(h);
            OptionChanged();
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return 0;
        }
        case ID_RL_CANCEL: DestroyWindow(h); return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        EnableWindow(g_hwnd, TRUE);
        if (g_rpWnd) EnableWindow(g_rpWnd, TRUE);
        SetForegroundWindow(g_hwnd);
        g_rulesWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static void OpenRules(const wstring& prefill = L"") {
    if (g_rulesWnd) { SetForegroundWindow(g_rulesWnd); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = RulesProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(col::panel);
        wc.lpszClassName = L"TextTidyRules";
        wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        reg = true;
    }
    RECT mr; GetWindowRect(g_hwnd, &mr);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int w = std::min<int>(S(860), wa.right - wa.left), hh = std::min<int>(S(560), wa.bottom - wa.top);
    int x = (mr.left + mr.right - w) / 2, y = (mr.top + mr.bottom - hh) / 2;
    g_rulesWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"TextTidyRules", L"바꾸기·지우기 목록", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                                 x, y, w, hh, g_hwnd, nullptr, g_hInst, nullptr);
    if (!g_rulesWnd) {
        DWORD err = GetLastError();
        MessageBoxW(g_hwnd, (L"목록 창을 열지 못했습니다 (오류 " + std::to_wstring(err) + L")").c_str(), L"야문 클린리더", MB_ICONERROR);
        return;
    }
    SetClassLongPtrW(g_rulesWnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_brPanel);
    ApplyTitleBar(g_rulesWnd);
    ApplyScrollTheme(g_rlList);
    if (!prefill.empty()) SetWindowTextW(g_rlFind, prefill.c_str());
    EnableWindow(g_hwnd, FALSE);
    if (g_rpWnd) EnableWindow(g_rpWnd, FALSE);  // 찾기·바꾸기 창에서 같은 목록을 따로 고치지 않도록
    ShowWindow(g_rulesWnd, SW_SHOW);
    SetFocus(g_rlFind);
}

// ---------------------------------------------------------------------------
// 찾기·바꾸기 창 (정리 결과 칸의 글을 직접 찾아 바꿈)
// 보이지 않는 서식 문자는 찾을 때 건너뛰고, 바꿀 때도 그 자리에 그대로 남김
static HWND g_rpFind, g_rpRepl, g_rpFindX, g_rpReplX, g_rpInfo;
static bool g_rpExpanded = false;
// 창을 닫았다 열어도 찾을 말·바꿀 말·옵션·기록은 그대로 (프로그램을 켜 둔 동안)
static wstring g_rpLastFind, g_rpLastRepl;
static std::vector<wstring> g_rpHistF, g_rpHistR;
static bool g_rpOpt[4] = {false, false, false, false};
static void RpSaveState() {
    auto txt = [](HWND c) { int n = GetWindowTextLengthW(c); wstring w(n + 1, 0); GetWindowTextW(c, &w[0], n + 1); w.resize(n); return w; };
    auto hist = [](HWND c) {
        std::vector<wstring> v;
        int n = (int)SendMessageW(c, CB_GETCOUNT, 0, 0);
        for (int i = 0; i < n; ++i) {
            int l = (int)SendMessageW(c, CB_GETLBTEXTLEN, i, 0);
            if (l <= 0) continue;
            wstring w(l + 1, 0);
            SendMessageW(c, CB_GETLBTEXT, i, (LPARAM)&w[0]);
            w.resize(l);
            v.push_back(w);
        }
        return v;
    };
    g_rpLastFind = txt(g_rpFind); g_rpLastRepl = txt(g_rpRepl);
    g_rpHistF = hist(g_rpFind); g_rpHistR = hist(g_rpRepl);
    int ids[] = {ID_RP_CASE, ID_RP_WORD, ID_RP_UP, ID_RP_REGEX};
    for (int i = 0; i < 4; ++i) g_rpOpt[i] = Checked(GetDlgItem(g_rpWnd, ids[i]));
}
static void RpLoadState(HWND h) {
    for (auto& w : g_rpHistF) SendMessageW(g_rpFind, CB_ADDSTRING, 0, (LPARAM)w.c_str());
    for (auto& w : g_rpHistR) SendMessageW(g_rpRepl, CB_ADDSTRING, 0, (LPARAM)w.c_str());
    SetWindowTextW(g_rpFind, g_rpLastFind.c_str());
    SetWindowTextW(g_rpRepl, g_rpLastRepl.c_str());
    int ids[] = {ID_RP_CASE, ID_RP_WORD, ID_RP_UP, ID_RP_REGEX};
    for (int i = 0; i < 4; ++i) SetToggle(GetDlgItem(h, ids[i]), g_rpOpt[i]);
}
static wstring g_rpUndoBefore, g_rpUndoAfter;  // 모두 바꾸기 되돌리기 (Ctrl+Z)
struct RpHit { size_t a, b; wstring rep; };
static bool RpWordCh(wchar_t c) {
    return iswalnum(c) || c == L'_' || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3131 && c <= 0x318E) || (c >= 0x4E00 && c <= 0x9FFF) ||
           (c >= 0x3040 && c <= 0x30FF);
}
static wchar_t RpFold(wchar_t c) { return (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : (c >= 0xFF21 && c <= 0xFF3A) ? (wchar_t)(c + 32) : c; }
static wstring RpGet(HWND combo, HWND multi) {
    HWND e = g_rpExpanded ? multi : combo;
    int n = GetWindowTextLengthW(e);
    wstring s(n + 1, 0);
    GetWindowTextW(e, &s[0], n + 1);
    s.resize(n);
    wstring o;  // 줄 끝은 칸 안과 같이 \r 하나로
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r' && i + 1 < s.size() && s[i + 1] == L'\n') { o += L'\r'; ++i; }
        else if (s[i] == L'\n') o += L'\r';
        else o += s[i];
    }
    return o;
}
// 결과 칸의 글 (칸 안의 위치와 같게 줄 끝은 \r)
static wstring RpHay() {
    wstring w = GetRichText(g_dst), o;
    o.reserve(w.size());
    for (size_t i = 0; i < w.size(); ++i) {
        if (w[i] == L'\r' && i + 1 < w.size() && w[i + 1] == L'\n') { o += L'\r'; ++i; }
        else o += w[i];
    }
    return o;
}
static bool RpSearch(const wstring& hay, std::vector<RpHit>& hits, wstring& err) {
    hits.clear();
    wstring q = RpGet(g_rpFind, g_rpFindX), repl = RpGet(g_rpRepl, g_rpReplX);
    wstring q2;
    for (wchar_t c : q) if (!IsInvisibleCh((char32_t)c)) q2 += c;
    q = q2;
    if (q.empty()) { err = L"찾을 말을 적어 주세요."; return false; }
    bool cs = Checked(GetDlgItem(g_rpWnd, ID_RP_CASE)), word = Checked(GetDlgItem(g_rpWnd, ID_RP_WORD));
    bool rx = Checked(GetDlgItem(g_rpWnd, ID_RP_REGEX));
    // 보이는 글자만 모은 글 + 원래 위치
    wstring vis;
    std::vector<size_t> vp;
    vis.reserve(hay.size()); vp.reserve(hay.size());
    for (size_t i = 0; i < hay.size(); ++i) if (!IsInvisibleCh((char32_t)hay[i])) { vis += hay[i]; vp.push_back(i); }
    auto wordOk = [&](size_t s, size_t e) {
        if (!word) return true;
        if (s > 0 && RpWordCh(vis[s - 1]) && RpWordCh(vis[s])) return false;
        if (e < vis.size() && RpWordCh(vis[e]) && RpWordCh(vis[e - 1])) return false;
        return true;
    };
    auto add = [&](size_t s, size_t e, const wstring& rep) {
        RpHit h; h.a = vp[s]; h.b = vp[e - 1] + 1; h.rep = rep;
        for (size_t k = h.a; k < h.b; ++k) if (IsInvisibleCh((char32_t)hay[k])) h.rep += hay[k];  // 서식 문자는 남김
        hits.push_back(h);
    };
    const size_t kMax = 200000;
    if (!rx) {
        wstring V = vis, Q = q;
        if (!cs) { for (auto& c : V) c = RpFold(c); for (auto& c : Q) c = RpFold(c); }
        size_t pos = 0;
        while (hits.size() < kMax) {
            size_t k = V.find(Q, pos);
            if (k == wstring::npos) break;
            if (wordOk(k, k + Q.size())) { add(k, k + Q.size(), repl); pos = k + Q.size(); }
            else pos = k + 1;
        }
        return true;
    }
    try {
        auto fl = std::regex::ECMAScript | (cs ? std::regex::flag_type(0) : std::regex::icase);
        // 정규식에 줄바꿈(\n 또는 여러 줄 칸의 줄바꿈)이 있으면 글 전체에서, 없으면 한 줄씩 찾음
        bool multi = q.find(L'\r') != wstring::npos || q.find(L"\\n") != wstring::npos;
        for (auto& c : q) if (c == L'\r') c = L'\n';
        std::wregex re(q, fl);
        auto fixRep = [](wstring r) {  // 바꿀 말의 \n 은 줄바꿈
            for (size_t k = 0; (k = r.find(L"\\n", k)) != wstring::npos;) r.replace(k, 2, L"\r");
            return r;
        };
        // 긴 글은 나눠서 찾음 (정규식 엔진이 아주 긴 글에서 멈추지 않도록)
        // 여러 줄: 빈 줄 묶음이 갈리지 않게 "줄바꿈 다음 글자" 자리에서 나눔 / 한 줄: 긴 줄은 공백에서 나눔
        wstring all = vis;
        if (multi) for (auto& c : all) if (c == L'\r') c = L'\n';
        auto run = [&](size_t base, size_t len) {
            wstring t = all.substr(base, len);
            for (std::wsregex_iterator it(t.begin(), t.end(), re), end; it != end && hits.size() < kMax; ++it) {
                const std::wsmatch& m = *it;
                if (m.length(0) == 0) continue;
                size_t s = base + (size_t)m.position(0), e = s + (size_t)m.length(0);
                if (!wordOk(s, e)) continue;
                if ((vis[s] >= 0xDC00 && vis[s] <= 0xDFFF) || (vis[e - 1] >= 0xD800 && vis[e - 1] <= 0xDBFF)) continue;  // 이모지 반쪽
                wstring r = m.format(repl);
                for (auto& c : r) if (c == L'\n') c = L'\r';
                add(s, e, fixRep(r));
            }
        };
        const size_t kChunk = 2000;
        if (multi) {
            size_t pos = 0, N = all.size();
            while (pos < N && hits.size() < kMax) {
                size_t end = std::min(N, pos + kChunk * 2);
                if (end < N) {
                    size_t k = end, lim = std::min(N, end + kChunk * 2);
                    while (k < lim && !(all[k - 1] == L'\n' && all[k] != L'\n')) ++k;
                    if (k < lim) end = k;
                }
                run(pos, end - pos);
                pos = end;
            }
            return true;
        }
        size_t ls = 0;
        while (ls <= vis.size() && hits.size() < kMax) {  // 한 줄씩 (^ $ 가 줄 처음·끝)
            size_t le = vis.find(L'\r', ls);
            if (le == wstring::npos) le = vis.size();
            size_t pos = ls;
            while (pos < le) {
                size_t end = le;
                if (le - pos > kChunk) {
                    end = pos + kChunk;
                    size_t k = end;
                    while (k > pos + kChunk / 2 && vis[k - 1] != L' ') --k;
                    if (k > pos + kChunk / 2) end = k;
                }
                run(pos, end - pos);
                pos = end;
            }
            ls = le + 1;
        }
    } catch (...) {
        err = L"정규식 문법이 맞지 않거나 너무 복잡합니다.";
        hits.clear();
        return false;
    }
    return true;
}
static void RpInfo(const wstring& s) { SetWindowTextW(g_rpInfo, s.c_str()); }
// 바꿀 수 있나: 정리 모드는 결과 칸 전체가 보일 때, 읽기 모드는 늘 (글 전체가 보이므로)
static bool RpEditable() { return !g_busy && !g_text.empty() && (g_reading || !g_preview); }
// 큰 글을 읽기 모드에서 바꾼 뒤: 저장·정리 모드 표시가 쓰는 정리 결과에도 반영
static void RpCommit() {
    if (!(g_reading && g_preview)) return;
    wstring w = GetRichText(g_dst);
    g_out = NormalizeNL(WToU32(w.data(), w.size()));
}
// 찾은 곳이 찾기·바꾸기 창에 가려지면 창을 글 칸의 위·아래 반대쪽으로 옮김
static void RpKeepVisible() {
    if (!g_rpWnd) return;
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    POINTL pl = {0, 0};
    SendMessageW(g_dst, EM_POSFROMCHAR, (WPARAM)&pl, cr.cpMin);
    POINT pt = {pl.x, pl.y};
    ClientToScreen(g_dst, &pt);
    RECT wr, dr; GetWindowRect(g_rpWnd, &wr); GetWindowRect(g_dst, &dr);
    int lh = S(40);
    if (pt.y + lh < wr.top || pt.y > wr.bottom || pt.x < wr.left - S(20) || pt.x > wr.right) return;
    int h = wr.bottom - wr.top;
    int top = (wr.top - dr.top < (dr.bottom - dr.top) / 3) ? dr.bottom - h - S(8) : dr.top + S(8);
    if (top < dr.top) top = dr.top;
    SetWindowPos(g_rpWnd, nullptr, wr.left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static void RpRemember(HWND combo) {  // 찾은 말 기록 (이번에 켠 동안)
    wchar_t buf[1024]; GetWindowTextW(combo, buf, 1024);
    if (!buf[0]) return;
    int at = (int)SendMessageW(combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)buf);
    if (at == 0) return;
    if (at > 0) SendMessageW(combo, CB_DELETESTRING, at, 0);
    SendMessageW(combo, CB_INSERTSTRING, 0, (LPARAM)buf);
    SetWindowTextW(combo, buf);
    while (SendMessageW(combo, CB_GETCOUNT, 0, 0) > 15) SendMessageW(combo, CB_DELETESTRING, 15, 0);
}
// 찾을 말을 적는 동안 바로 찾아 보여 줌 (지금 고른 곳부터)
static void RpFindIncr() {
    wstring hay = RpHay(), err;
    std::vector<RpHit> hits;
    if (RpGet(g_rpFind, g_rpFindX).empty()) { RpInfo(L""); return; }
    if (!RpSearch(hay, hits, err)) { RpInfo(err); return; }
    if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return; }
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    int pick = 0;
    for (size_t i = 0; i < hits.size(); ++i) if ((LONG)hits[i].a >= cr.cpMin) { pick = (int)i; break; }
    SendMessageW(g_dst, EM_SETSEL, hits[pick].a, hits[pick].b);
    SendMessageW(g_dst, EM_SCROLLCARET, 0, 0);
    RpKeepVisible();
    RpInfo(std::to_wstring(pick + 1) + L" / " + std::to_wstring(hits.size()) + L"곳");
}
static bool RpFindNext(bool quiet = false) {
    wstring hay = RpHay(), err;
    std::vector<RpHit> hits;
    if (!RpSearch(hay, hits, err)) { RpInfo(err); return false; }
    if (!g_rpExpanded) RpRemember(g_rpFind);
    if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return false; }
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    bool up = Checked(GetDlgItem(g_rpWnd, ID_RP_UP));
    int pick = -1;
    bool wrapped = false;
    if (!up) {
        for (size_t i = 0; i < hits.size(); ++i) if ((LONG)hits[i].a >= cr.cpMax) { pick = (int)i; break; }
        if (pick < 0) { pick = 0; wrapped = true; }
    } else {
        for (size_t i = hits.size(); i-- > 0;) if ((LONG)hits[i].a < cr.cpMin) { pick = (int)i; break; }
        if (pick < 0) { pick = (int)hits.size() - 1; wrapped = true; }
    }
    const RpHit& hh = hits[pick];
    SendMessageW(g_dst, EM_SETSEL, hh.a, hh.b);
    SendMessageW(g_dst, EM_SCROLLCARET, 0, 0);
    RpKeepVisible();
    if (!quiet || wrapped)
        RpInfo(std::to_wstring(pick + 1) + L" / " + std::to_wstring(hits.size()) + L"곳" +
               (wrapped ? (up ? L"   (처음까지 찾아서 끝에서부터 다시 찾음)" : L"   (끝까지 찾아서 처음부터 다시 찾음)") : L""));
    return true;
}
static void RpReplaceOne() {
    if (!RpEditable()) { RpInfo(L"큰 파일은 정리 모드에서 앞부분만 보이므로 바꾸기는 읽기 모드에서 하세요."); return; }
    wstring hay = RpHay(), err;
    std::vector<RpHit> hits;
    if (!RpSearch(hay, hits, err)) { RpInfo(err); return; }
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    for (auto& hh : hits) {
        if ((LONG)hh.a == cr.cpMin && (LONG)hh.b == cr.cpMax) {  // 지금 고른 곳이 찾은 말이면 바꿈
            bool ro = (GetWindowLongW(g_dst, GWL_STYLE) & ES_READONLY) != 0;  // 읽기 모드 칸은 잠깐 풀었다가 다시 잠금
            if (ro) SendMessageW(g_dst, EM_SETREADONLY, FALSE, 0);
            SendMessageW(g_dst, EM_REPLACESEL, TRUE, (LPARAM)hh.rep.c_str());
            if (ro) SendMessageW(g_dst, EM_SETREADONLY, TRUE, 0);
            g_dstDirty = true;
            RpCommit();
            RpFindNext(true);
            if (GetWindowTextLengthW(g_rpInfo) == 0 || true) {
                std::vector<RpHit> left; wstring e2;
                RpSearch(RpHay(), left, e2);
                RpInfo(L"1곳을 바꿨습니다.  남은 곳: " + std::to_wstring(left.size()));
            }
            return;
        }
    }
    RpFindNext();
}
static void RpRememberBoth() { if (!g_rpExpanded) { RpRemember(g_rpFind); RpRemember(g_rpRepl); } }
static void RpReplaceAll() {
    if (!RpEditable()) { RpInfo(L"큰 파일은 정리 모드에서 앞부분만 보이므로 바꾸기는 읽기 모드에서 하세요."); return; }
    wstring hay = RpHay(), err;
    std::vector<RpHit> hits;
    if (!RpSearch(hay, hits, err)) { RpInfo(err); return; }
    if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return; }
    wstring before = GetRichText(g_dst), out;
    out.reserve(hay.size());
    size_t at = 0;
    for (auto& hh : hits) { out.append(hay, at, hh.a - at); out += hh.rep; at = hh.b; }
    out.append(hay, at, wstring::npos);
    wstring crlf;  // 칸에 넣을 글: 줄 끝 \r\n
    crlf.reserve(out.size() + out.size() / 20);
    for (wchar_t c : out) { if (c == L'\r') crlf += L"\r\n"; else crlf += c; }
    long top = TopChar(g_dst);
    SetRichText(g_dst, crlf);
    ScrollToChar(g_dst, top);
    g_dstDirty = true;
    RpCommit();
    g_rpUndoBefore = before;
    g_rpUndoAfter = GetRichText(g_dst);
    RpInfo(std::to_wstring(hits.size()) + L"곳을 바꿨습니다" + (hits.size() >= 200000 ? L" (한 번에 20만 곳까지, 다시 누르면 이어서 바꿈)" : L"") + L".  되돌리려면 결과 칸에서 Ctrl+Z");
    SetStatus(std::to_wstring(hits.size()) + L"곳을 바꿨습니다. 저장해야 파일에 남습니다.");
}
// 결과 칸 Ctrl+Z: 바로 앞에 한 "모두 바꾸기" 되돌리기
static bool RpUndoAll() {
    if (g_rpUndoAfter.empty()) return false;
    if (GetRichText(g_dst) != g_rpUndoAfter) { g_rpUndoAfter.clear(); g_rpUndoBefore.clear(); return false; }
    long top = TopChar(g_dst);
    wstring b = g_rpUndoBefore;
    g_rpUndoAfter.clear(); g_rpUndoBefore.clear();
    SetRichText(g_dst, b);
    ScrollToChar(g_dst, top);
    g_dstDirty = true;
    RpCommit();
    SetStatus(L"모두 바꾸기를 되돌렸습니다.");
    if (g_rpWnd) RpInfo(L"모두 바꾸기를 되돌렸습니다.");
    return true;
}
static void RpAddRule() {
    wstring f = RpGet(g_rpFind, g_rpFindX), t = RpGet(g_rpRepl, g_rpReplX);
    if (f.empty()) { RpInfo(L"찾을 말을 적어 주세요."); return; }
    if (f.find(L'\r') != wstring::npos || t.find(L'\r') != wstring::npos) { RpInfo(L"여러 줄로 된 말은 규칙으로 저장할 수 없습니다."); return; }
    bool rx = Checked(GetDlgItem(g_rpWnd, ID_RP_REGEX));
    RuleItem r; r.kind = rx ? 2 : 0; r.f = f; r.t = t; r.on = true;
    if (!RuleStorable(r)) { RpInfo(L"이 말은 규칙으로 저장할 수 없습니다. (앞뒤 공백, #로 시작하는 말, => 가 든 말)"); return; }
    if (rx) { try { std::wregex re(f, std::regex::ECMAScript); } catch (...) { RpInfo(L"정규식 문법이 맞지 않습니다."); return; } }
    if (g_dstDirty && !g_text.empty() &&
        MessageBoxW(g_rpWnd, L"규칙으로 저장하면 글을 다시 정리합니다.\n결과 칸에서 직접 고친 내용은 없어집니다. 계속할까요?", L"규칙으로 저장", MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
        return;
    std::vector<wstring> cm;
    std::vector<RuleItem> v = RulesFromText(LoadRulesText(), &cm);
    for (auto& x : v) if (x.kind == r.kind && x.f == r.f && x.t == r.t) { RpInfo(L"이미 목록에 있는 규칙입니다."); return; }
    v.push_back(r);
    if (!SaveRuleItems(v, cm, g_rpWnd)) return;
    RpInfo(L"규칙으로 저장했습니다. 이 글과 앞으로 정리하는 모든 글에 자동으로 적용됩니다.");
    OptionChanged();
}
static RECT g_rpOptBox;  // 옵션 묶음 테두리
static void RpLayout(HWND h) {
    RECT rc; GetClientRect(h, &rc);
    int W = rc.right, p = S(18), lw = S(58), bw = S(132), bh = S(30), gap = S(8);
    int bx = W - p - bw, fx = p + lw, fw = bx - S(16) - fx;
    MoveWindow(g_rpFind, fx, S(18), fw, S(300), TRUE);
    MoveWindow(g_rpRepl, fx, S(18) + bh + gap, fw, S(300), TRUE);
    RECT cr; GetWindowRect(g_rpFind, &cr);
    int ch = cr.bottom - cr.top;  // 선택목록 실제 높이에 맞춰 이름표를 가운데로
    MoveWindow(GetDlgItem(h, ID_RP_L1), p, S(18) + (bh - ch) / 2 + (ch - S(20)) / 2, lw, S(20), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_L2), p, S(18) + bh + gap + (bh - ch) / 2 + (ch - S(20)) / 2, lw, S(20), TRUE);
    MoveWindow(g_rpFind, fx, S(18) + (bh - ch) / 2, fw, S(300), TRUE);
    MoveWindow(g_rpRepl, fx, S(18) + bh + gap + (bh - ch) / 2, fw, S(300), TRUE);
    int oy = S(18) + 2 * (bh + gap) + S(4), ow = bx - S(16) - p, cw = (ow - S(24)) / 2;
    g_rpOptBox = {p, oy, p + ow, oy + S(66)};
    MoveWindow(GetDlgItem(h, ID_RP_CASE), p + S(12), oy + S(8), cw, S(24), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_UP), p + S(12) + cw, oy + S(8), cw, S(24), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_WORD), p + S(12), oy + S(36), cw, S(24), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_REGEX), p + S(12) + cw, oy + S(36), cw, S(24), TRUE);
    MoveWindow(g_rpInfo, p + S(2), oy + S(72), ow, S(20), TRUE);
    int ids[] = {ID_RP_NEXT, ID_RP_REPLACE, ID_RP_ALL, ID_RP_COUNT, ID_RP_EXPAND};
    for (int i = 0; i < 5; ++i) MoveWindow(GetDlgItem(h, ids[i]), bx, S(18) + i * (bh + gap), bw, bh, TRUE);
    int ey = S(18) + 5 * (bh + gap) + S(6);
    if (g_rpExpanded) {
        int eh = (rc.bottom - ey - S(16) - S(48)) / 2;
        MoveWindow(GetDlgItem(h, ID_RP_L3), p, ey, W - p * 2, S(22), TRUE);
        MoveWindow(g_rpFindX, p, ey + S(24), W - p * 2, eh, TRUE);
        MoveWindow(GetDlgItem(h, ID_RP_L4), p, ey + S(24) + eh + S(4), W - p * 2, S(22), TRUE);
        MoveWindow(g_rpReplX, p, ey + S(24) + eh + S(28), W - p * 2, eh, TRUE);
    }
    for (int id : {ID_RP_L3, ID_RP_L4}) ShowWindow(GetDlgItem(h, id), g_rpExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_rpFindX, g_rpExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_rpReplX, g_rpExpanded ? SW_SHOW : SW_HIDE);
    EnableWindow(g_rpFind, !g_rpExpanded);
    EnableWindow(g_rpRepl, !g_rpExpanded);
}
static int RpClientH(bool expanded) { return S(18) + 5 * (S(30) + S(8)) + S(10) + (expanded ? S(300) : 0); }
static void RpSetExpanded(HWND h, bool on) {
    if (on == g_rpExpanded) return;
    // 한 줄 칸 ↔ 여러 줄 칸 내용 옮기기
    if (on) {
        wchar_t buf[1024];
        GetWindowTextW(g_rpFind, buf, 1024); SetWindowTextW(g_rpFindX, buf);
        GetWindowTextW(g_rpRepl, buf, 1024); SetWindowTextW(g_rpReplX, buf);
    }
    g_rpExpanded = on;
    SetWindowTextW(GetDlgItem(h, ID_RP_EXPAND), on ? L"축소" : L"확장");
    RECT wr; GetWindowRect(h, &wr);
    RECT cr = {0, 0, 10, RpClientH(on)};
    AdjustWindowRectEx(&cr, (DWORD)GetWindowLongW(h, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(h, GWL_EXSTYLE));
    int nh = cr.bottom - cr.top, ny = wr.top;
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (ny + nh > wa.bottom) ny = std::max<int>(wa.top, wa.bottom - nh);  // 화면 아래로 넘치지 않게
    SetWindowPos(h, nullptr, wr.left, ny, wr.right - wr.left, nh, SWP_NOZORDER);
    RpLayout(h);
    SetFocus(on ? g_rpFindX : g_rpFind);
    if (on) {  // 옮겨 온 글은 골라 둠: 바로 Enter를 치면 그 글 대신 줄바꿈이 들어감
        SendMessageW(g_rpFindX, EM_SETSEL, 0, -1);
        SendMessageW(g_rpReplX, EM_SETSEL, 0, -1);
    }
}
static void RpUpdateState() {
    if (!g_rpWnd) return;
    bool ed = RpEditable();
    for (int id : {ID_RP_REPLACE, ID_RP_ALL}) EnableWindow(GetDlgItem(g_rpWnd, id), ed);
    if (!ed && !g_text.empty() && g_preview)
        RpInfo(L"큰 파일은 정리 모드에서 앞부분만 보이므로 바꾸기는 읽기 모드에서 하세요.");
}
static LRESULT CALLBACK RpProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_rpWnd = h;
        auto btn = [&](int id, const wchar_t* t) {
            HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SetWindowSubclass(b, BtnProc, 0, 0);
            return b;
        };
        auto lab = [&](int id, const wchar_t* t, HFONT f) {
            HWND s = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(s, WM_SETFONT, (WPARAM)f, TRUE);
            return s;
        };
        auto combo = [&](int id) {
            HWND c = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL,
                                     0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            return c;
        };
        auto multi = [&](int id) {
            HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                     0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(e, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            return e;
        };
        lab(ID_RP_L1, L"찾을 말", g_uiFont);
        lab(ID_RP_L2, L"바꿀 말", g_uiFont);
        g_rpFind = combo(ID_RP_FIND);
        g_rpRepl = combo(ID_RP_REPL);
        btn(ID_RP_CASE, L"대소문자 구분");
        btn(ID_RP_WORD, L"온전한 단어만");
        btn(ID_RP_UP, L"위쪽으로 찾기");
        btn(ID_RP_REGEX, L"정규식");
        g_rpInfo = lab(ID_RP_INFO, L"", g_smallFont);
        btn(ID_RP_NEXT, L"찾기");
        btn(ID_RP_REPLACE, L"바꾸기");
        btn(ID_RP_ALL, L"모두 바꾸기");
        btn(ID_RP_COUNT, L"횟수");
        btn(ID_RP_EXPAND, L"확장");
        lab(ID_RP_L3, L"찾을 말 (여러 줄)", g_uiFont);
        g_rpFindX = multi(ID_RP_FINDX);
        lab(ID_RP_L4, L"바꿀 말 (여러 줄)", g_uiFont);
        g_rpReplX = multi(ID_RP_REPLX);
        RpLoadState(h);
        for (HWND c : {g_rpFind, g_rpRepl}) ApplyCtlTheme(c, L"DarkMode_CFD");
        for (HWND c : {g_rpFindX, g_rpReplX}) ApplyScrollTheme(c);
        RpLayout(h);
        return 0;
    }
    case WM_SIZE: RpLayout(h); InvalidateRect(h, nullptr, TRUE); return 0;
    case WM_ERASEBKGND: {  // 바탕 + 창 테두리 + 옵션 묶음 테두리
        HDC dc = (HDC)wp;
        RECT rc; GetClientRect(h, &rc);
        FillRect2(dc, rc, col::panel);
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillRound(g, g_rpOptBox, S(6), col::panel, col::border);
        g.SetSmoothingMode(SmoothingModeNone);
        Pen edge(GC(col::boxBorder), 1.0f);
        g.DrawRectangle(&edge, 0, 0, rc.right - 1, rc.bottom - 1);
        return 1;
    }
    case WM_DRAWITEM: DrawButton((const DRAWITEMSTRUCT*)lp); return TRUE;
    case WM_CTLCOLORSTATIC: {
        SetBkColor((HDC)wp, col::panel);
        SetTextColor((HDC)wp, GetDlgCtrlID((HWND)lp) == ID_RP_INFO ? col::okText : col::text);
        return (LRESULT)g_brPanel;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        SetBkColor((HDC)wp, col::field);
        SetTextColor((HDC)wp, col::text);
        return (LRESULT)g_brField;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE) RpUpdateState();
        break;
    case WM_TIMER:
        if (wp == 7) { KillTimer(h, 7); if (!g_text.empty()) RpFindIncr(); return 0; }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDCANCEL) { DestroyWindow(h); return 0; }
        if ((id == ID_RP_FIND && (HIWORD(wp) == CBN_EDITCHANGE || HIWORD(wp) == CBN_SELCHANGE)) || (id == ID_RP_FINDX && HIWORD(wp) == EN_CHANGE)) {
            SetTimer(h, 7, 300, nullptr);  // 적는 동안 바로 찾기 (잠깐 멈추면)
            return 0;
        }
        if (id == IDOK) id = ID_RP_NEXT;  // Enter: 찾기
        else if (HIWORD(wp) != BN_CLICKED) break;
        if (g_text.empty() && id != ID_RP_LIST && id != ID_RP_EXPAND && id != ID_RP_CASE && id != ID_RP_WORD && id != ID_RP_UP && id != ID_RP_REGEX && id != ID_RP_RULE) {
            RpInfo(L"먼저 파일을 열거나 글을 붙여넣으세요.");
            return 0;
        }
        switch (id) {
        case ID_RP_CASE: case ID_RP_WORD: case ID_RP_UP: case ID_RP_REGEX: {
            HWND b = GetDlgItem(h, id); SetToggle(b, !Checked(b)); return 0;
        }
        case ID_RP_NEXT: RpFindNext(); return 0;
        case ID_RP_REPLACE: RpRememberBoth(); RpReplaceOne(); return 0;
        case ID_RP_ALL: RpRememberBoth(); RpReplaceAll(); return 0;
        case ID_RP_COUNT: {
            wstring err; std::vector<RpHit> hits;
            if (!RpSearch(RpHay(), hits, err)) RpInfo(err);
            else RpInfo(hits.empty() ? L"찾는 말이 없습니다." : std::to_wstring(hits.size()) + L"곳 있습니다." + (g_preview && !g_reading ? L"  (화면에 보이는 앞부분 기준)" : L""));
            return 0;
        }
        case ID_RP_RULE: RpAddRule(); return 0;
        case ID_RP_LIST: {
            wstring pre = g_rpExpanded ? L"" : RpGet(g_rpFind, g_rpFindX);
            OpenRules(pre);
            return 0;
        }
        case ID_RP_EXPAND: RpSetExpanded(h, !g_rpExpanded); return 0;
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: RpSaveState(); g_rpWnd = nullptr; g_rpExpanded = false; SetFocus(g_dst); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static void OpenReplace() {
    if (g_rpWnd) {
        ShowWindow(g_rpWnd, SW_SHOW);
        SetForegroundWindow(g_rpWnd);
    } else {
        static bool reg = false;
        if (!reg) {
            WNDCLASSEXW wc = {sizeof(wc)};
            wc.lpfnWndProc = RpProc;
            wc.hInstance = g_hInst;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.lpszClassName = L"YamoonReplace";
            wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
            RegisterClassExW(&wc);
            reg = true;
        }
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        RECT cr = {0, 0, S(560), RpClientH(false)};
        AdjustWindowRectEx(&cr, style, FALSE, WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW);
        RECT dr; GetWindowRect(g_dst, &dr);  // 글 칸 위 오른쪽에 띄움
        int w = cr.right - cr.left, hh = cr.bottom - cr.top;
        int x = dr.right - w - S(24), y = dr.top + S(8);
        RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        if (x < wa.left) x = wa.left;
        if (y + hh > wa.bottom) y = wa.bottom - hh;
        if (y < wa.top) y = wa.top;
        HWND rw = CreateWindowExW(WS_EX_DLGMODALFRAME, L"YamoonReplace", L"찾기·바꾸기", style, x, y, w, hh, g_hwnd, nullptr, g_hInst, nullptr);
        if (!rw) return;
        SetClassLongPtrW(rw, GCLP_HBRBACKGROUND, (LONG_PTR)g_brPanel);
        ApplyTitleBar(rw, true);  // 글 칸과 구분되게 테두리를 강조색으로
        ShowWindow(rw, SW_SHOW);
    }
    // 결과 칸에서 고른 말이 있으면 찾을 말로
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    if (cr.cpMax > cr.cpMin && cr.cpMax - cr.cpMin < 200) {
        wstring hay = RpHay();
        if ((size_t)cr.cpMax <= hay.size()) {
            wstring s = hay.substr(cr.cpMin, cr.cpMax - cr.cpMin);
            if (s.find(L'\r') == wstring::npos) SetWindowTextW(g_rpFind, s.c_str());
        }
    }
    RpUpdateState();
    SetFocus(g_rpExpanded ? g_rpFindX : g_rpFind);
    SendMessageW(g_rpFind, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
}

// ---------------------------------------------------------------------------
// 프로그램 정보 창
static HWND g_aboutWnd;
static void RethemeSubWindows() {  // 열려 있는 보조 창도 새 테마로
    if (g_rulesWnd && IsWindow(g_rulesWnd)) {
        SetClassLongPtrW(g_rulesWnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_brPanel);
        ApplyTitleBar(g_rulesWnd);
        ListView_SetBkColor(g_rlList, col::field); ListView_SetTextBkColor(g_rlList, col::field); ListView_SetTextColor(g_rlList, col::text);
        ApplyScrollTheme(g_rlList);
        RedrawWindow(g_rulesWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    if (g_bwWnd && IsWindow(g_bwWnd)) {
        ListView_SetBkColor(g_bwList, col::field); ListView_SetTextBkColor(g_bwList, col::field); ListView_SetTextColor(g_bwList, col::text);
        ApplyScrollTheme(g_bwList);
        ApplyCtlTheme((HWND)SendMessageW(g_bwList, LVM_GETHEADER, 0, 0), L"DarkMode_ItemsView");  // 목록 머리줄도 어둡게
        ApplyTitleBar(g_bwWnd);
        RedrawWindow(g_bwWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    if (g_rpWnd && IsWindow(g_rpWnd)) {
        SetClassLongPtrW(g_rpWnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_brPanel);
        ApplyTitleBar(g_rpWnd, true);
        for (HWND c : {g_rpFind, g_rpRepl}) ApplyCtlTheme(c, L"DarkMode_CFD");
        for (HWND c : {g_rpFindX, g_rpReplX}) ApplyScrollTheme(c);
        RedrawWindow(g_rpWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    if (g_aboutWnd && IsWindow(g_aboutWnd)) { ApplyTitleBar(g_aboutWnd); RedrawWindow(g_aboutWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN); }
}
static LRESULT CALLBACK AboutProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HWND ok = CreateWindowExW(0, L"BUTTON", L"확인", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, h,
                                  (HMENU)(INT_PTR)ID_ABOUT_OK, g_hInst, nullptr);
        SetWindowSubclass(ok, BtnProc, 0, 0);
        RECT rc; GetClientRect(h, &rc);
        MoveWindow(ok, rc.right - S(24) - S(96), rc.bottom - S(20) - S(34), S(96), S(34), FALSE);
        SetFocus(ok);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int W = rc.right, H = rc.bottom;
        HDC dc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HGDIOBJ ob = SelectObject(dc, bmp);
        FillRect2(dc, rc, col::win);
        // 위쪽 옅은 띠
        RECT band = {0, 0, W, S(118)};
        FillRect2(dc, band, col::panel);
        RECT bl = {0, S(118), W, S(119)};
        FillRect2(dc, bl, col::panelLine);
        HICON ico = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, S(64), S(64), 0);
        if (ico) { DrawIconEx(dc, S(28), S(27), ico, S(64), S(64), 0, nullptr, DI_NORMAL); DestroyIcon(ico); }
        int x = S(110);
        RECT r1 = {x, S(30), W - S(20), S(62)};
        DrawTxt(dc, kAppName, r1, g_aboutTitle, col::text);
        RECT r2 = {x, S(64), W - S(20), S(86)};
        DrawTxt(dc, wstring(L"버전 ") + kVersion + L"   ·   " + kDate, r2, g_uiFont, col::sub);
        RECT r2b = {x, S(84), W - S(20), S(104)};
        DrawTxt(dc, L"Windows 7 이상  ·  설치 없이 실행", r2b, g_uiFont, col::sub);

        int y = S(138), lx = S(28), rw = W - S(56);
        auto para = [&](const wstring& t, HFONT f, COLORREF c, int gapAfter) {
            RECT m = {lx, y, lx + rw, y};
            HGDIOBJ o = SelectObject(dc, f);
            DrawTextW(dc, t.c_str(), (int)t.size(), &m, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
            SelectObject(dc, o);
            RECT d = {lx, y, lx + rw, m.bottom};
            DrawTxt(dc, t, d, f, c, DT_WORDBREAK | DT_NOPREFIX);
            y = m.bottom + gapAfter;
        };
        para(L"흐트러진 텍스트 파일(EPUB 전자책 포함)의 문단을 읽기 좋게 정리하는 프로그램입니다. "
             L"옛 모바일용 고정폭 글, 한 줄로 이어진 글, 쓸데없이 많은 줄바꿈, "
             L"공백·문장부호·특수문자 문제를 자동으로 찾아 정리하고, 정리한 글은 읽기 모드에서 편하게 읽을 수 있습니다.", g_uiFont, col::text, S(16));
        // 제작 정보 (라벨 / 값)
        auto kv = [&](const wchar_t* k, const wstring& v) {
            RECT kr = {lx, y, lx + S(64), y + S(22)};
            DrawTxt(dc, k, kr, g_uiFont, col::sub);
            RECT vr = {lx + S(64), y, lx + rw, y + S(22)};
            DrawTxt(dc, v, vr, g_uiFont, col::text);
            y += S(22);
        };
        kv(L"제작", L"야문 짱  &  Claude  (협동작)");
        kv(L"버전", wstring(kVersion) + L"  (" + kDate + L")");
        kv(L"빌드", BuildDisplay());
        kv(L"라이선스", L"MIT License (무료)");
        y += S(14);
        RECT sep = {lx, y, lx + rw, y + 1};
        FillRect2(dc, sep, col::panelLine);
        y += S(12);
        para(L"이 프로그램은 누구나 무료로 사용할 수 있습니다. "
             L"원본 파일은 수정하지 않고 '_정리.txt'로 따로 저장하지만, 중요한 파일은 미리 백업해 두시기 바랍니다. "
             L"이 프로그램은 있는 그대로 제공되며, 사용으로 인해 생긴 문제에 대해 제작자는 책임지지 않습니다.",
             g_smallFont, col::sub, S(10));
        para(L"Copyright © 2026 야문 짱. MIT License.", g_smallFont, col::sub, 0);

        BitBlt(hdc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DRAWITEM:
        DrawButton((const DRAWITEMSTRUCT*)lp);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_ABOUT_OK) { DestroyWindow(h); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        EnableWindow(g_hwnd, TRUE);
        SetForegroundWindow(g_hwnd);
        g_aboutWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static void OpenAbout() {
    if (g_aboutWnd) { SetForegroundWindow(g_aboutWnd); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = AboutProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"YamoonAbout";
        wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        reg = true;
    }
    RECT cr = {0, 0, S(480), S(452)};
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRectEx(&cr, style, FALSE, WS_EX_DLGMODALFRAME);
    int w = cr.right - cr.left, hh = cr.bottom - cr.top;
    RECT mr; GetWindowRect(g_hwnd, &mr);
    int x = (mr.left + mr.right - w) / 2, y = (mr.top + mr.bottom - hh) / 2;
    g_aboutWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"YamoonAbout", (wstring(kAppName) + L" 정보").c_str(), style,
                                 x, y, w, hh, g_hwnd, nullptr, g_hInst, nullptr);
    if (!g_aboutWnd) return;
    ApplyTitleBar(g_aboutWnd);
    EnableWindow(g_hwnd, FALSE);
    ShowWindow(g_aboutWnd, SW_SHOW);
}

static void ClearAll();
static void CopyResult();
static void PasteAsSource() {
    if (!OpenClipboard(g_hwnd)) return;
    HANDLE hd = GetClipboardData(CF_UNICODETEXT);
    wstring w;
    if (hd) {
        const wchar_t* p = (const wchar_t*)GlobalLock(hd);
        if (p) { w = p; GlobalUnlock(hd); }
    }
    CloseClipboard();
    if (w.empty()) return;
    g_text = NormalizeNL(WToU32(w.data(), w.size()));
    g_enc = E_UTF8BOM;
    g_epub = false;
    g_path.clear();
    g_mergeFiles.clear();
    g_mergeBounds.clear();
    g_lastSaved.clear();
    ShowSource();
    UpdateTitle();
    Reprocess();
    AfterTextChange();
    SetStatus(L"붙여넣은 글을 정리했습니다. 저장하면 파일 이름을 묻습니다.");
}

// 원본·결과 칸 비우기 (처음 상태)
static void ClearAll() {
    if (g_busy) return;
    if (!g_text.empty() && !ConfirmDiscardEdits()) return;
    SaveReadPos();
    g_posPath.clear();
    g_newDoc = true;
    g_text.clear(); g_out.clear(); g_outToSrc.clear(); g_srcToOut.clear();
    g_path.clear(); g_mergeFiles.clear(); g_mergeBounds.clear(); g_lastSaved.clear();
    g_preview = false;
    g_epub = false;
    SendMessageW(g_src, EM_SETREADONLY, FALSE, 0);
    SendMessageW(g_dst, EM_SETREADONLY, g_reading ? TRUE : FALSE, 0);
    SetRichText(g_src, L"");
    SetRichText(g_dst, L"");
    g_srcStale = false;
    g_dstDirty = false; g_srcPending = false;
    g_metaSrc.clear(); g_metaDst.clear(); g_sizeText.clear(); g_chips.clear();
    UpdateTitle();
    AfterTextChange();
    Layout();
    InvalidateRect(g_hwnd, nullptr, FALSE);
    SetStatus(L"원본과 결과를 비웠습니다. 새 글을 붙여넣거나 파일을 여세요.");
    SetFocus(g_reading ? g_dst : g_src);
}
// 정리 결과 전체를 클립보드로 (큰 파일도 전체)
static void CopyResult() {
    if (g_text.empty() || g_busy) return;
    wstring w = (g_preview || !g_dstDirty) ? U32ToW(g_out, true) : GetRichText(g_dst);
    while (!w.empty() && (w.back() == L'\n' || w.back() == L'\r')) w.pop_back();
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (hg) {
        wchar_t* p = (wchar_t*)GlobalLock(hg);
        memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(hg);
        if (!SetClipboardData(CF_UNICODETEXT, hg)) GlobalFree(hg);
    }
    CloseClipboard();
    SetStatus(L"정리 결과를 모두 복사했습니다. 원하는 곳에 붙여넣으세요.");
    g_statusOk = true;
    InvalidateRect(g_hwnd, &g_rcStatus, FALSE);
}
// 글 칸 오른쪽 단추 메뉴
static void ShowEditMenu(HWND ed, LPARAM lp) {
    bool src = ed == g_src;
    bool ro = g_preview || (g_reading && ed == g_dst);
    CHARRANGE cr; SendMessageW(ed, EM_EXGETSEL, 0, (LPARAM)&cr);
    bool sel = cr.cpMax > cr.cpMin;
    bool clip = IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | ((sel && !ro) ? 0 : MF_GRAYED), ID_M_CUT, L"잘라내기\tCtrl+X");
    AppendMenuW(m, MF_STRING | (sel ? 0 : MF_GRAYED), ID_M_COPY, L"복사\tCtrl+C");
    AppendMenuW(m, MF_STRING | ((clip && !ro) ? 0 : MF_GRAYED), ID_M_PASTE, L"붙여넣기\tCtrl+V");
    AppendMenuW(m, MF_STRING, ID_M_SELALL, L"모두 선택\tCtrl+A");
    if (!src) AppendMenuW(m, MF_STRING | (g_text.empty() ? MF_GRAYED : 0), ID_REPLACE, L"찾기·바꾸기…\tCtrl+H");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    if (src) AppendMenuW(m, MF_STRING | (clip ? 0 : MF_GRAYED), ID_PASTE, L"클립보드 글로 바꿔서 정리");
    else AppendMenuW(m, MF_STRING | (g_text.empty() ? MF_GRAYED : 0), ID_COPYALL, L"정리 결과 모두 복사");
    AppendMenuW(m, MF_STRING | (g_text.empty() ? MF_GRAYED : 0), ID_CLEAR, L"원본·결과 비우기");
    POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (pt.x == -1 && pt.y == -1) { RECT r; GetWindowRect(ed, &r); pt.x = r.left + S(20); pt.y = r.top + S(20); }
    SetFocus(ed);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
        case ID_M_CUT: SendMessageW(ed, WM_CUT, 0, 0); break;
        case ID_M_COPY: SendMessageW(ed, WM_COPY, 0, 0); break;
        case ID_M_PASTE: SendMessageW(ed, EM_PASTESPECIAL, CF_UNICODETEXT, 0); break;
        case ID_M_SELALL: SendMessageW(ed, EM_SETSEL, 0, -1); break;
        case ID_PASTE: PasteAsSource(); break;
        case ID_COPYALL: CopyResult(); break;
        case ID_REPLACE: OpenReplace(); break;
        case ID_CLEAR: ClearAll(); break;
    }
}

// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = h;
        ApplyTitleBar(h);
        CreateUI();
        DragAcceptFiles(h, TRUE);
        Layout();
        SetStatus(L"파일을 끌어다 놓거나 [열기]를 누르세요. 원본 칸에 글을 붙여넣어도 됩니다.");
        return 0;
    }
    case WM_SIZE:
        Layout();
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = S(1050);
        mm->ptMinTrackSize.y = S(500);
        {   // 옵션 칸(정리)·보기 설정 줄(읽기)이 두 줄로 바뀌지 않는 폭까지만 줄어들게
            int needClient = std::max(g_panelNeedW, g_viewNeedW);
            RECT r = {0, 0, needClient, 100};
            AdjustWindowRectEx(&r, (DWORD)GetWindowLongW(h, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(h, GWL_EXSTYLE));
            if (needClient > 0 && r.right - r.left > mm->ptMinTrackSize.x) mm->ptMinTrackSize.x = r.right - r.left;
        }
        {   // 화면(작업 영역)보다 크게 요구하지 않음 — 좁은 화면 + 높은 배율 대비
            RECT wa;
            if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
                if (mm->ptMinTrackSize.x > wa.right - wa.left) mm->ptMinTrackSize.x = wa.right - wa.left;
                if (mm->ptMinTrackSize.y > wa.bottom - wa.top) mm->ptMinTrackSize.y = wa.bottom - wa.top;
            }
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        Paint(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* m = (MEASUREITEMSTRUCT*)lp;
        if (m->CtlType == ODT_COMBOBOX) { m->itemHeight = (m->itemID == (UINT)-1) ? S(20) : S(24); return TRUE; }  // 닫힌 칸 20 + 테두리 = 26
        break;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* d = (const DRAWITEMSTRUCT*)lp;
        if (d->CtlType == ODT_COMBOBOX) DrawComboItem(d);
        else DrawButton(d);
        return TRUE;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, col::field);
        SetTextColor(dc, col::text);
        return (LRESULT)g_brField;
    }
    case WM_CTLCOLORSTATIC: {  // 주 창에서는 흐리게 된 글자 수 칸
        HDC dc = (HDC)wp;
        SetBkColor(dc, col::fieldDis);
        SetTextColor(dc, col::faint);
        return (LRESULT)g_brFieldDis;
    }
    case WM_ACTIVATEAPP:
        if (!wp) HideTip();  // 다른 프로그램으로 가면 풍선 도움말을 닫음
        break;
    case WM_SETTINGCHANGE:
        if (g_themeMode == 0 && lp && lstrcmpiW((const wchar_t*)lp, L"ImmersiveColorSet") == 0 && SystemDark() != g_dark) SetTheme(0);
        break;
    case WM_DROPFILES:
        HandleDrop((HDROP)wp);
        DragFinish((HDROP)wp);
        AfterTextChange();
        return 0;
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)lp;
        if (nh->code == EN_DROPFILES && (nh->idFrom == ID_SRC || nh->idFrom == ID_DST)) {
            ENDROPFILES* ed = (ENDROPFILES*)lp;
            HandleDrop((HDROP)ed->hDrop);
            AfterTextChange();
            return 0;
        }
        if (nh->code == EN_SELCHANGE && !g_syncing) {
            SELCHANGE* sc = (SELCHANGE*)lp;
            if (nh->idFrom == ID_SRC && GetFocus() == g_src) SyncFromSrc(sc->chrg);
            else if (nh->idFrom == ID_DST && GetFocus() == g_dst) SyncFromDst(sc->chrg);
        }
        break;
    }
    case WM_CONTEXTMENU:
        if ((HWND)wp == g_src || (HWND)wp == g_dst) { ShowEditMenu((HWND)wp, lp); return 0; }
        break;
#ifdef MEASURE
    case WM_APP_MEAS:
        meas::Finish();
        return 0;
#endif
    case WM_APP_PROCDONE:
        OnProcDone((ProcJob*)lp);
        return 0;
    case WM_TIMER:
#ifdef MEASURE
        if (wp == 77) { meas::Tick(); return 0; }
#endif
        if (wp == TIMER_READ) {  // 읽기 모드: 읽은 곳 %
            int p = ReadPercent();
            if (p != g_readPct) { g_readPct = p; InvalidateRect(h, g_fullscreen ? &g_rcDstHead : &g_rcStatus, FALSE); }
            return 0;
        }
        KillTimer(h, wp);
        if (wp == TIMER_SAVED) { g_savedFlash = false; InvalidateRect(g_btnSave, nullptr, FALSE); return 0; }
        if (wp == TIMER_LONG) {
            if (SplitMode() == 0) OptionChanged();
        } else if (wp == TIMER_SRC) {
            wstring w = GetRichText(g_src);
            g_text = NormalizeNL(WToU32(w.data(), w.size()));
            g_mergeBounds.clear();  // 직접 고치면 파일 경계를 알 수 없으므로 한 글로 정리
            g_srcPending = false;
            Reprocess();
            AfterTextChange();
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (lp && (code == CBN_DROPDOWN || code == CBN_CLOSEUP)) InvalidateRect((HWND)lp, nullptr, FALSE);
        if (code == CBN_DROPDOWN) HideTip();  // 펼친 목록 위에 풍선 도움말이 겹치지 않게
        switch (id) {
        case ID_OPEN: DoOpen(); AfterTextChange(); return 0;
        case ID_RECENT: ShowRecentMenu(g_btnOpen); AfterTextChange(); return 0;
        case ID_THEME: ShowThemeMenu(g_btnTheme); return 0;
        case ID_SAVE: DoSave(); return 0;
        case ID_SAVEAS: if (!g_text.empty()) DoSaveAs(); return 0;
        case ID_BATCH: DoBatchDialog(); AfterTextChange(); return 0;
        case ID_PASTE: if (!g_busy && ConfirmDiscardEdits()) PasteAsSource(); return 0;
        case ID_CLEAR: ClearAll(); return 0;
        case ID_COPYALL: CopyResult(); return 0;
        case ID_READ: SetReading(true); return 0;
        case ID_RTOC: if (g_reading) ShowTocMenu(g_btnToc); return 0;
        case ID_RFIND: OpenReplace(); return 0;  // 정리·읽기 모드 모두 같은 찾기·바꾸기 창
        case ID_REPLACE: OpenReplace(); return 0;
        case ID_RMARK: if (g_reading) ShowMarkMenu(g_btnMark); return 0;
        case ID_MARK_ADD: AddMark(); return 0;
        case ID_RFULL: SetFullscreen(!g_fullscreen); return 0;
        case ID_SIZE_UP: if (g_reading) StepFontSize(1); return 0;
        case ID_SIZE_DOWN: if (g_reading) StepFontSize(-1); return 0;
        case ID_MODE_TIDY: SetReading(false); return 0;
        case ID_VRESET:  // 보기 설정을 처음 상태로
            if (MessageBoxW(h, L"읽기 모드의 글꼴·크기·굵게·색·줄 간격·여백을 처음 상태로 되돌릴까요?", L"보기 초기화",
                            MB_OKCANCEL | MB_ICONQUESTION) != IDOK) { SetFocus(g_dst); return 0; }
            {
                const wchar_t* defFont = L"맑은 고딕";
                auto it = std::find(g_fontNames.begin(), g_fontNames.end(), wstring(defFont));
                if (it == g_fontNames.end()) { g_fontNames.push_back(defFont); SendMessageW(g_cbVFont, CB_ADDSTRING, 0, (LPARAM)defFont); it = g_fontNames.end() - 1; }
                g_vFont = defFont; g_vSize = 11; g_vColor = 0; g_vLine = 1; g_vMargin = 1; g_vBold = false;
                InvalidateRect(g_btnVBold, nullptr, FALSE);
                SendMessageW(g_cbVFont, CB_SETCURSEL, (WPARAM)(it - g_fontNames.begin()), 0);
                int si = 0;
                for (int i = 0; i < (int)(sizeof(kSizes) / sizeof(*kSizes)); ++i) if (kSizes[i] == 11) si = i;
                SendMessageW(g_cbVSize, CB_SETCURSEL, si, 0);
                SendMessageW(g_cbVColor, CB_SETCURSEL, 0, 0);
                SendMessageW(g_cbVLine, CB_SETCURSEL, 1, 0);
                SendMessageW(g_cbVMargin, CB_SETCURSEL, 1, 0);
                SaveView();
                ApplyView(true, true);
                SetStatus(L"보기 설정을 처음 상태로 되돌렸습니다.");
            }
            SetFocus(g_dst);
            return 0;
        case ID_VBOLD:
            g_vBold = !g_vBold;
            InvalidateRect(g_btnVBold, nullptr, FALSE);
            SaveView();
            ApplyView(true, true);
            SetFocus(g_dst);
            return 0;
        case ID_VFG: PickViewColor(true); SetFocus(g_dst); return 0;
        case ID_VBG: PickViewColor(false); SetFocus(g_dst); return 0;
        case ID_VFONT: case ID_VSIZE: case ID_VCOLOR: case ID_VLINE: case ID_VMARGIN:
            if (code == CBN_SELCHANGE) {
                int sel = CbSel((HWND)lp);
                bool fontChanged = false;
                if (id == ID_VFONT && sel >= 0 && sel < (int)g_fontNames.size()) { g_vFont = g_fontNames[sel]; fontChanged = true; }
                if (id == ID_VSIZE && sel >= 0 && sel < (int)(sizeof(kSizes) / sizeof(*kSizes))) { g_vSize = kSizes[sel]; fontChanged = true; }
                if (id == ID_VCOLOR) g_vColor = sel;
                if (id == ID_VLINE) g_vLine = sel;
                if (id == ID_VMARGIN) g_vMargin = sel;
                SaveView();
                ApplyView(fontChanged, id == ID_VLINE);
                SetFocus(g_dst);
            }
            return 0;
        case ID_ABOUT: OpenAbout(); return 0;
        case ID_FOLD:
            g_fold = !g_fold;
            Layout();  // [접기] / [옵션 펼치기] 글자도 Layout에서 바꿈
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case ID_RESET:
            if (MessageBoxW(h, L"모든 옵션을 처음 설치했을 때의 상태로 되돌릴까요?\n(바꾸기·지우기 목록은 그대로 둡니다)",
                            L"옵션 초기화", MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
                return 0;
            if (!ConfirmDiscardEdits()) return 0;
            ResetOptions();
            return 0;
        case ID_READENC:
            if (code == CBN_SELCHANGE) {
                g_forceEnc = CbSel(g_cbReadEnc);
                if (!g_path.empty() && ConfirmDiscardEdits()) Reload();
            }
            return 0;
        case ID_BLANK: case ID_JOIN: case ID_SPACE: case ID_SPLIT: case ID_PARASEP: case ID_SCENE: case ID_INDENT: case ID_QUOTE: case ID_ELLIPSIS:
            if (code == CBN_SELCHANGE) { if (id == ID_SPLIT) { EnableWindow(g_edLong, SplitMode() == 0); InvalidateRect(h, &g_rcLongBox, FALSE); } if (ConfirmDiscardEdits()) OptionChanged(); }
            return 0;
        case ID_COLLAPSE: case ID_JUNK: case ID_SYNC: case ID_HANJA: case ID_HALF:
            if (code == BN_CLICKED || code == BN_DOUBLECLICKED) {
                HWND t = (HWND)lp;
                if (id != ID_SYNC && !ConfirmDiscardEdits()) return 0;
                SetToggle(t, !Checked(t));
                if (id != ID_SYNC) OptionChanged();
            }
            return 0;
        case ID_LONGLEN:
            if (code == EN_CHANGE) SetTimer(h, TIMER_LONG, 500, nullptr);
            if (code == EN_SETFOCUS || code == EN_KILLFOCUS) InvalidateRect(h, &g_rcLongBox, FALSE);
            return 0;
        case ID_SRC:
            if (code == EN_CHANGE && !g_loading) { DmUpdate(g_src); g_srcPending = true; SetTimer(h, TIMER_SRC, 400, nullptr); }
            return 0;
        case ID_DST:
            if (code == EN_CHANGE && !g_loading) { DmUpdate(g_dst); g_dstDirty = true; }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (g_fullscreen) SetFullscreen(false);
        SaveReadPos();
        SaveSettings();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

#ifdef MEASURE
// 측정 끝: 첫 화면 그리기와 남은 줄 나눔까지 잰 뒤 결과를 보여 주고 기록 파일에 덧붙임
void meas::LogLine(const wstring& line) {
    if (logPath.empty()) {
        size_t sl = g_ini.find_last_of(L"\\/");
        logPath = (sl == wstring::npos ? wstring() : g_ini.substr(0, sl + 1)) + L"YamoonCleanReader_측정기록.txt";
    }
    bool fresh = GetFileAttributesW(logPath.c_str()) == INVALID_FILE_ATTRIBUTES;
    HANDLE fh = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE) return;
    SetFilePointer(fh, 0, nullptr, FILE_END);
    int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), (int)line.size(), nullptr, 0, nullptr, nullptr);
    std::string u8(fresh ? "\xEF\xBB\xBF" : "");
    size_t at = u8.size();
    u8.resize(at + n);
    WideCharToMultiByte(CP_UTF8, 0, line.c_str(), (int)line.size(), &u8[at], n, nullptr, nullptr);
    DWORD wr = 0;
    WriteFile(fh, u8.data(), (DWORD)u8.size(), &wr, nullptr);
    CloseHandle(fh);
}
static const UINT_PTR kMeasTimer = 77;
// 1단계: 첫 화면까지 잰 뒤, 화면이 멈추는지·뒤에서 하는 줄 나눔이 언제 끝나는지 지켜봄 (억지로 끝내게 하지 않음)
void meas::Finish() {
    if (!on || mon) return;
    Mark(L"(앞 단계 뒤 대기 중이던 다른 처리)");
    RedrawWindow(g_hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
    Mark(L"첫 화면 그리기");
    mon = true;
    maxGap = 0;
    QueryPerformanceCounter(&monStart);
    monLast = monChange = monStart;
    SCROLLINFO si = {sizeof(si), SIF_RANGE};
    lastMax = GetScrollInfo(g_dst, SB_VERT, &si) ? si.nMax : -1;
    firstFlipMs = -1;
    flipPending = true;
    SetTimer(g_hwnd, kMeasTimer, 15, nullptr);
}
// 2단계: 15ms마다. 틱 사이가 길면 그동안 화면이 멈춘 것. 스크롤 막대 범위가 2초 동안 그대로면 줄 나눔이 끝난 것으로 봄
void meas::Tick() {
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    double gap = Ms(monLast, n);
    if (gap > maxGap) maxGap = gap;
    monLast = n;
    SCROLLINFO si = {sizeof(si), SIF_RANGE};
    int mx = GetScrollInfo(g_dst, SB_VERT, &si) ? si.nMax : -1;
    if (mx != lastMax) { lastMax = mx; monChange = n; }
    if (Ms(monChange, n) < 2000 && Ms(monStart, n) < 30000) return;
    KillTimer(g_hwnd, kMeasTimer);
    mon = false;
    on = false;
    double bg = Ms(monStart, monChange);
    long lines = (long)SendMessageW(g_dst, EM_GETLINECOUNT, 0, 0);  // 이미 끝났으면 바로 나옴

    double toFirst = 0;
    for (auto& st : steps) toFirst += st.second;
    wstring fileLine;
    if (!g_path.empty()) {
        size_t sl = g_path.find_last_of(L"\\/");
        fileLine = L"파일: " + (sl == wstring::npos ? g_path : g_path.substr(sl + 1));
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_path.c_str(), GetFileExInfoStandard, &fa))
            fileLine += L"  (" + SizeText(((long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) + L", " + (g_epub ? wstring(L"EPUB") : wstring(kEncName[g_enc])) + L")";
    }
    RECT dr; GetClientRect(g_dst, &dr);
    auto sec = [](double ms) { long long t = (long long)(ms / 100 + 0.5); return std::to_wstring(t / 10) + L"." + std::to_wstring(t % 10) + L"초"; };
    wstring r = L"[" + title + L"]\r\n";
    if (!fileLine.empty()) r += fileLine + L"\r\n";
    r += L"읽기 글꼴: " + g_vFont + L" " + std::to_wstring(g_vSize) + L"pt  ·  결과 칸 폭 " + std::to_wstring(dr.right - dr.left) + L"px  ·  화면 줄 수 " + Num(lines) + L"줄\r\n";
    for (auto& nt : notes) r += nt + L"\r\n";
    r += L"\r\n★ 열기 → 첫 화면: " + Num((long long)(toFirst + 0.5)) + L" ms (" + sec(toFirst) + L")\r\n";
    r += L"★ 첫 화면 뒤 줄 나눔이 끝나기까지: 약 " + Num((long long)(bg + 0.5)) + L" ms" + (bg < 50 ? L" (바로 끝남)" : L"") + L"\r\n";
    r += L"★ 그동안 화면이 가장 오래 멈춘 시간: " + Num((long long)(maxGap + 0.5)) + L" ms\r\n";
    r += L"\r\n단계별 시간 (1ms 미만은 생략)\r\n";
    for (auto& st : steps) {
        if (st.second < 1.0) continue;
        r += L"  " + Num((long long)(st.second + 0.5)) + L" ms   " + st.first + L"\r\n";
    }
    SYSTEMTIME tm; GetLocalTime(&tm);
    wchar_t ts[64];
    swprintf(ts, 64, L"===== %04d-%02d-%02d %02d:%02d:%02d =====\r\n", tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond);
    LogLine(wstring(ts) + r + L"\r\n");
    if (toFirst + bg < 300) return;  // 금방 끝난 것은 기록 파일에만
    wstring q = r + L"\r\n(실행 파일 옆 YamoonCleanReader_측정기록.txt에도 저장됐습니다. 이 창에서 Ctrl+C를 누르면 글이 복사됩니다.)\r\n"
                    L"창을 닫은 뒤 바로 Space를 눌러 첫 쪽 넘김이 얼마나 걸리는지도 기록됩니다.\r\n\r\n";
    MessageBoxW(g_hwnd, q.c_str(), L"야문 클린리더 측정판", MB_OK | MB_ICONINFORMATION);
}
#endif

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int nShow) {
    g_hInst = hi;
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    LoadLibraryW(L"Msftedit.dll");
    GdiplusStartupInput gsi;
    ULONG_PTR gtoken = 0;
    GdiplusStartup(&gtoken, &gsi, nullptr);

    wchar_t exe[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
    wstring ep(exe, n);
    size_t d = ep.find_last_of(L'.');
    wstring base = d == wstring::npos ? ep : ep.substr(0, d);
    size_t sl = ep.find_last_of(L"\\/");
    wstring dir = sl == wstring::npos ? L"" : ep.substr(0, sl + 1);
    g_ini = dir + L"YamoonCleanReader.ini";
    g_rulesPath = dir + L"YamoonCleanReader_rules.txt";
    // 예전 이름(문단 정리기)의 설정·규칙 파일이 있으면 그대로 이어 씀
    CopyFileW((dir + L"YamoonTextTidy.ini").c_str(), g_ini.c_str(), TRUE);
    (void)base;

    HDC sdc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSY);
    ReleaseDC(nullptr, sdc);
    g_themeMode = IniGet(L"theme", 0);
    if (g_themeMode < 0 || g_themeMode > 2) g_themeMode = 0;
    ApplyThemeColors(g_themeMode == 0 ? SystemDark() : g_themeMode == 2);

    auto font = [&](int pt10, int weight) {
        return CreateFontW(-MulDiv(pt10, g_dpi, 720), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"맑은 고딕");
    };
    g_uiFont = font(85, FW_NORMAL);
    g_smallFont = font(80, FW_NORMAL);
    g_sectFont = font(80, FW_BOLD);
    g_btnFont = font(90, FW_NORMAL);
    g_btnBold = font(90, FW_BOLD);
    g_cardFont = font(90, FW_BOLD);
    g_titleFont = font(115, FW_BOLD);
    g_dropFont = g_titleFont;
    g_aboutTitle = font(160, FW_BOLD);
    {   // 보기 설정 불러오기
        wchar_t fb[LF_FACESIZE] = {};
        GetPrivateProfileStringW(L"opt", L"vfont", L"맑은 고딕", fb, LF_FACESIZE, g_ini.c_str());
        g_vFont = fb[0] ? fb : L"맑은 고딕";
        g_vSize = IniGet(L"vsize", 11);
        if (g_vSize < 6 || g_vSize > 72) g_vSize = 11;
        {   // 색 조합 (예전 번호는 새 목록 번호로 바꿔 읽음)
            int v2 = IniGet(L"vcolor2", -1);
            if (v2 < 0) { static const int old2new[] = {0, 2, 3, 7, 8}; int o = IniGet(L"vcolor", 0); v2 = (o >= 0 && o <= 4) ? old2new[o] : 0; }
            g_vColor = (v2 >= 0 && v2 <= kColorCustom) ? v2 : 0;
        }
        g_vfg = (COLORREF)GetPrivateProfileIntW(L"opt", L"vfg", RGB(30, 30, 30), g_ini.c_str());
        g_vbg = (COLORREF)GetPrivateProfileIntW(L"opt", L"vbg", RGB(255, 255, 255), g_ini.c_str());
        {   // 줄 간격 (예전 번호 0~3은 "좁게"가 앞에 들어간 새 목록 번호 1~4로 바꿔 읽음)
            int v2 = IniGet(L"vline2", -1);
            if (v2 < 0) { int o = IniGet(L"vline", 0); v2 = (o >= 0 && o <= 3) ? o + 1 : 1; }
            g_vLine = (v2 >= 0 && v2 <= 4) ? v2 : 1;
        }
        g_vBold = IniGet(L"vbold", 0) != 0;
        g_vMargin = IniGet(L"vmargin", 1); if (g_vMargin < 0 || g_vMargin > 2) g_vMargin = 1;
        g_reading = IniGet(L"reading", 0) != 0;
        for (auto& c : g_custCols) c = RGB(255, 255, 255);
        // 한글 글꼴 목록 (세로쓰기용 @글꼴 제외)
        HDC fdc = GetDC(nullptr);
        LOGFONTW lf = {};
        lf.lfCharSet = HANGUL_CHARSET;
        EnumFontFamiliesExW(fdc, &lf, [](const LOGFONTW* l, const TEXTMETRICW*, DWORD, LPARAM p) -> int {
            auto* v = (std::vector<wstring>*)p;
            wstring n = l->lfFaceName;
            if (!n.empty() && n[0] != L'@' && std::find(v->begin(), v->end(), n) == v->end()) v->push_back(n);
            return 1;
        }, (LPARAM)&g_fontNames, 0);
        ReleaseDC(nullptr, fdc);
        std::sort(g_fontNames.begin(), g_fontNames.end());
        if (std::find(g_fontNames.begin(), g_fontNames.end(), g_vFont) == g_fontNames.end()) g_fontNames.insert(g_fontNames.begin(), g_vFont);
    }
    g_edFont = font(IniGet(L"fontsize", 11) * 10, FW_NORMAL);  // 정리 모드 글꼴
    g_readFont = MakeReadFont();                                // 읽기 모드 글꼴


    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"TextTidyWnd";
    RegisterClassExW(&wc);

    int w = S(IniGet(L"w", 1320)), hh = S(IniGet(L"h", 820));
    {   // 화면보다 크지 않게 (1024x768 등 작은 화면)
        RECT wa;
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
            int aw = wa.right - wa.left, ah = wa.bottom - wa.top;
            if (w > aw) w = aw;
            if (hh > ah) hh = ah;
        }
    }
    HWND hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, L"TextTidyWnd", kAppName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, w, hh, nullptr, nullptr, hi, nullptr);
    ShowWindow(hwnd, IniGet(L"maximized", 0) ? SW_SHOWMAXIMIZED : nShow);
    UpdateWindow(hwnd);
    if (g_reading) SetTimer(hwnd, TIMER_READ, 300, nullptr);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        std::vector<wstring> files;
        for (int i = 1; i < argc; ++i) if (FileExists(argv[i])) files.push_back(argv[i]);
        LocalFree(argv);
        std::sort(files.begin(), files.end(), [](const wstring& a, const wstring& b) { return StrCmpLogicalW(FileName(a).c_str(), FileName(b).c_str()) < 0; });
        if (files.size() == 1) LoadFile(files[0]);
        else if (files.size() > 1) OpenBatchWindow(files);
        AfterTextChange();
    }

    ACCEL acc[] = {
        {FCONTROL | FVIRTKEY, 'O', ID_OPEN},
        {FCONTROL | FVIRTKEY, 'S', ID_SAVE},
        {FCONTROL | FSHIFT | FVIRTKEY, 'S', ID_SAVEAS},
        {FVIRTKEY, VK_F1, ID_ABOUT},
        {FVIRTKEY, VK_F11, ID_RFULL},
        {FCONTROL | FVIRTKEY, 'F', ID_RFIND},
        {FCONTROL | FVIRTKEY, 'H', ID_REPLACE},
        {FCONTROL | FVIRTKEY, 'B', ID_MARK_ADD},
        {FCONTROL | FVIRTKEY, VK_OEM_PLUS, ID_SIZE_UP},
        {FCONTROL | FVIRTKEY, VK_ADD, ID_SIZE_UP},
        {FCONTROL | FVIRTKEY, VK_OEM_MINUS, ID_SIZE_DOWN},
        {FCONTROL | FVIRTKEY, VK_SUBTRACT, ID_SIZE_DOWN},
    };
    HACCEL hacc = CreateAcceleratorTableW(acc, (int)(sizeof(acc) / sizeof(acc[0])));
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (g_aboutWnd && m.message == WM_KEYDOWN && (m.wParam == VK_RETURN || m.wParam == VK_ESCAPE)) {
            DestroyWindow(g_aboutWnd);
            continue;
        }
        if (g_rulesWnd && IsDialogMessageW(g_rulesWnd, &m)) continue;
        if (g_rpWnd && IsDialogMessageW(g_rpWnd, &m)) continue;
        if (g_bwWnd && m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE && !g_bwRunning) { DestroyWindow(g_bwWnd); continue; }
        if (!g_rulesWnd && !g_aboutWnd && !g_bwWnd && TranslateAcceleratorW(hwnd, hacc, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    GdiplusShutdown(gtoken);
    return 0;
}
