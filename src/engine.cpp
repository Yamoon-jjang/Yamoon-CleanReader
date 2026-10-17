// TextTidy - 문단 정리 엔진
#include "engine.h"
#include <vector>
#include <cstdint>
#include <regex>
#include <map>
#include <algorithm>

namespace {

typedef char32_t C;
typedef std::u32string S;

// 공백으로 보이는 글자: 일반 공백, 탭, 전각 공백, NBSP 등 유니코드 공백과
// 한글 채움 문자(ㅤ U+3164, ﾠ U+FFA0, 초성·중성 채움)와 점자 공백(⠀)처럼 "빈칸"으로 쓰이는 글자
bool isWS(C c) {
    return c == ' ' || c == '\t' || c == 0x3000 || c == 0xA0 || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x202F || c == 0x205F || c == 0x1680 || c == 0x3164 || c == 0xFFA0 || c == 0x115F ||
           c == 0x1160 || c == 0x2800;
}
// 보이지 않는 서식 문자(결합자·방향 표시·소프트 하이픈·이체 선택자 등). 원문 그대로 두되,
// 문장 끝·줄 첫머리 판단 같은 곳에서는 없는 것처럼 건너뛰고 봄
bool isInvisible(C c) {
    return c == 0x200B || c == 0x200C || c == 0x200D || c == 0xFEFF || c == 0x2060 || c == 0xAD || c == 0x180E ||
           c == 0x200E || c == 0x200F || (c >= 0x202A && c <= 0x202E) || (c >= 0x2061 && c <= 0x2064) ||
           (c >= 0x2066 && c <= 0x2069) || (c >= 0xFE00 && c <= 0xFE0F);
}
// 보이지 않는 글자를 건너뛴 마지막/첫 글자
C lastVis(const S& t) { for (size_t i = t.size(); i > 0; --i) if (!isInvisible(t[i - 1])) return t[i - 1]; return 0; }
C firstVis(const S& t) { for (C c : t) if (!isInvisible(c)) return c; return 0; }
bool isHangul(C c) { return (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3130 && c <= 0x318F) || (c >= 0x1100 && c <= 0x11FF); }
bool isHangulSyl(C c) { return c >= 0xAC00 && c <= 0xD7A3; }
bool isKanaHan(C c) { return (c >= 0x3040 && c <= 0x30FF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF); }
bool isAsciiAlpha(C c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDigit(C c) { return (c >= '0' && c <= '9') || (c >= 0xFF10 && c <= 0xFF19); }
bool isLetterDigit(C c) {
    if (c < 0x80) return isAsciiAlpha(c) || isDigit(c);
    return isHangul(c) || isKanaHan(c) || isDigit(c) || (c >= 0xC0 && c <= 0x24F) || (c >= 0x370 && c <= 0x4FF) ||
           (c >= 0xFF21 && c <= 0xFF3A) || (c >= 0xFF41 && c <= 0xFF5A) || (c >= 0x2160 && c <= 0x217F);
}
// 옛 모바일 txt는 CP949 바이트 수로 잘랐으므로 그 기준의 폭
int cw(C c) {
    if (c < 0x80) return 1;
    if (isInvisible(c)) return 0;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2000 && c <= 0x2BFF) || (c >= 0x2E80 && c <= 0xA4CF) ||
        (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6) || c >= 0x20000 || c == 0xB7)
        return 2;
    return 1;
}
int widthOf(const S& s) { int w = 0; for (C c : s) w += cw(c); return w; }

bool isStrongTerm(C c) { return c == '.' || c == '!' || c == '?' || c == 0x3002 || c == 0xFF01 || c == 0xFF1F || c == 0xFF0E; }
bool isWeakTerm(C c) {
    return c == 0x2026 || c == 0x2025 || c == '~' || c == 0xFF5E || c == 0x2661 || c == 0x2665 || c == 0x266A ||
           c == 0x266C || c == 0x2606 || c == 0x2605;
}
bool isTermChar(C c) { return isStrongTerm(c) || isWeakTerm(c); }
bool isOpenQuote(C c) { return c == '"' || c == 0x201C || c == 0x2018 || c == '\'' || c == 0x300C || c == 0x300E; }
bool isCloseQuote(C c) { return c == '"' || c == 0x201D || c == 0x2019 || c == '\'' || c == 0x300D || c == 0x300F; }
bool isCloseBracket(C c) { return c == ')' || c == ']' || c == 0x3011 || c == 0x3009 || c == 0x300B || c == 0xFF09; }

// 문장 끝으로 볼 만한 한글 어미
bool isEndingSyl(C c) {
    static const char32_t e[] = U"다요죠까네군지야아어래냐해라자니오";
    for (const char32_t* p = e; *p; ++p) if (*p == c) return true;
    return false;
}
bool isLineEndSyl(C c) {  // 줄 끝이 이것이면 문장이 끝났다고 봄(구두점 없는 글)
    static const char32_t e[] = U"다요죠까네";
    for (const char32_t* p = e; *p; ++p) if (*p == c) return true;
    return false;
}

bool startsWith(const S& s, size_t pos, const char32_t* w) {
    size_t i = 0;
    for (; w[i]; ++i) if (pos + i >= s.size() || s[pos + i] != w[i]) return false;
    return true;
}
bool startsWithCI(const S& s, const char* w) {
    size_t i = 0;
    for (; w[i]; ++i) {
        if (i >= s.size()) return false;
        C c = s[i]; if (c >= 'A' && c <= 'Z') c += 32;
        if (c != (C)w[i]) return false;
    }
    return true;
}

// 0: 끝나지 않음, 1: 약하게 끝남(어미·괄호), 2: 문장부호로 끝남
int endKind(const S& t) {
    if (t.empty()) return 0;
    C c = lastVis(t);
    if (!c) return 0;
    if (isCloseQuote(c)) return 2;
    if (isCloseBracket(c)) return 1;
    if (isTermChar(c)) return 2;
    if (isLineEndSyl(c)) return 1;
    if (c >= 0x3131 && c <= 0x318E) return 1;  // ㅋㅋ ㅎㅎ ㅠㅠ
    return 0;
}

bool isSymbolLine(const S& t) {
    for (C c : t) if (isLetterDigit(c)) return false;
    return !t.empty();
}

bool isHeading(const S& t, int w) {
    if (t.empty() || w > 40) return false;
    C c = firstVis(t);
    bool pat = false;
    if (isDigit(c)) {
        // "1화", "3. 제목", "12장" 등. 소수("3.5리 정도…")는 제목 번호가 아님
        size_t k = 0; while (k < t.size() && (isInvisible(t[k]) || isWS(t[k]))) ++k;
        while (k < t.size() && isDigit(t[k])) ++k;
        pat = !(k + 1 < t.size() && t[k] == '.' && isDigit(t[k + 1]));
    }
    else if (c == 0xC81C /*제*/ && t.size() > 1 && (isDigit(t[1]) || (t[1] == ' ' && t.size() > 2 && isDigit(t[2])))) pat = true;
    else if (c == 0x7B2C /*第*/ || c == '#' || c == '<' || c == 0x3008 || c == 0x300A || c == '[' || c == 0x3010 || c == '=' ||
             c == '-' || c == '+' || c == '*' || c == 0x25C6 || c == 0x25C7 || c == 0x25A0 || c == 0x25A1 || c == 0x25CF ||
             c == 0x25CB || c == 0x2605 || c == 0x2606 || c == 0x203B || c == 0x25B6 || c == 0x25B7 || c == 0x25BA)
        pat = true;
    else if (startsWithCI(t, "prologue") || startsWithCI(t, "epilogue") || startsWithCI(t, "chapter") ||
             startsWithCI(t, "episode") || startsWithCI(t, "part") || startsWithCI(t, "ep.") || startsWithCI(t, "ch."))
        pat = true;
    else {
        static const char32_t* kw[] = {U"프롤로그", U"에필로그", U"외전", U"후기", U"작가", U"서장", U"종장",
                                       U"막간", U"번외", U"특별편", U"공지", U"챕터", U"목차", U"序", nullptr};
        for (int i = 0; kw[i]; ++i) if (startsWith(t, 0, kw[i])) { pat = true; break; }
    }
    if (!pat) return false;
    return endKind(t) != 2 || w <= 10;
}

struct Line {
    S t;
    std::vector<int> m;  // t[k]의 원본 위치
    int lead = 0;       // 앞 공백 폭
    bool trail = false; // 뒤에 공백이 있었음
    int gap = 0;        // 앞에 있던 빈 줄 수
    int w = 0;
    int rw = 0;         // 앞 공백 포함 원래 폭(고정폭 판정용)
    bool head = false, sym = false;
    bool afterJunk = false;  // 바로 앞에서 쪽 번호·광고 줄을 지웠음 (그 간격은 장면 전환이 아님)
    bool dz = false;    // 이 줄 뒤의 빈 줄 1개는 문단 구분이 아니라 끊긴 줄 사이 (글 일부만 "줄마다 빈 줄"인 구간)
    bool lj = false;    // 이 줄 둘레에 끊긴 줄이 많음 (글 일부만 끊긴 구간): 글 전체 판정과 상관없이 이음
    int  run = 0;       // 빈 줄 없이 이어진 줄 묶음의 줄 수
};

// ---------- 문장 나누기 ----------
bool isAbbrev(const S& p, size_t dot) {
    // dot 위치의 '.' 앞 단어가 약어/이니셜인지
    size_t b = dot;
    while (b > 0 && isAsciiAlpha(p[b - 1])) --b;
    size_t len = dot - b;
    if (len == 0) return false;
    if (len == 1 && p[b] >= 'A' && p[b] <= 'Z') return true;  // J. K.
    S word = p.substr(b, len);
    for (auto& ch : word) if (ch >= 'A' && ch <= 'Z') ch += 32;
    static const char32_t* ab[] = {U"mr", U"mrs", U"ms", U"dr", U"st", U"vs", U"etc", U"no", U"jr", U"sr", U"prof", U"e", U"i", nullptr};
    for (int i = 0; ab[i]; ++i) if (word == ab[i]) return true;
    return false;
}

bool attachedAfterQuote(const S& p, size_t pos) {
    static const char32_t* w[] = {U"하고", U"하며", U"하면서", U"하는", U"하자", U"하곤", U"하던", U"하더니", U"하니",
                                  U"라고", U"라며", U"라는", U"라면서", U"이라고", U"이라며", U"이라는", U"란 ", U"이란 ", nullptr};
    for (int i = 0; w[i]; ++i) if (startsWith(p, pos, w[i])) return true;
    return false;
}

struct Range { size_t a, len; };
// 따옴표·괄호 짝 찾기. role: 0 보통 글자(짝 없는 따옴표 포함), 1 여는 것, 2 닫는 것
// startQ: 앞 문단에서 열린 채 넘어온 따옴표 (0 없음, 1 곧은 ", 2 둥근 “ 「 『)
// endQ: 이 문단 끝에서 열린 채 남은 따옴표 (다음 문단으로 이어지는 대사)
static const size_t kMaxOpenTail = 300;  // 닫히지 않은 따옴표가 이보다 길게 이어지면 따옴표로 보지 않음
bool isBrOpen(C c) {
    return c == '(' || c == '[' || c == 0xFF08 || c == 0xFF3B || c == 0x3010 || c == 0x3014 || c == 0x300A || c == 0x3008 ||
           c == 0x3016 || c == 0xAB || c == 0x2039;
}
C brCloseOf(C c) {
    switch (c) {
        case '(': return ')'; case '[': return ']'; case 0xFF08: return 0xFF09; case 0xFF3B: return 0xFF3D;
        case 0x3010: return 0x3011; case 0x3014: return 0x3015; case 0x300A: return 0x300B; case 0x3008: return 0x3009;
        case 0x3016: return 0x3017; case 0xAB: return 0xBB; case 0x2039: return 0x203A;
    }
    return 0;
}
bool isBrClose(C c) {
    return c == ')' || c == ']' || c == 0xFF09 || c == 0xFF3D || c == 0x3011 || c == 0x3015 || c == 0x300B || c == 0x3009 ||
           c == 0x3017 || c == 0xBB || c == 0x203A;
}
C qCloseOf(C c) { return c == 0x201C ? 0x201D : c == 0x2018 ? 0x2019 : c == 0x300C ? 0x300D : c == 0x300E ? 0x300F : 0; }
bool isQClose(C c) { return c == 0x201D || c == 0x2019 || c == 0x300D || c == 0x300F; }
void encloseRoles(const S& p, int startQ, std::vector<char>& role, int& endQ) {
    size_t n = p.size();
    role.assign(n, 0);
    endQ = 0;
    // 곧은 따옴표: 차례로 여닫음
    {
        bool inside = startQ == 1;
        size_t lastOpen = SIZE_MAX;
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != '"') continue;
            if (inside) { role[i] = 2; inside = false; lastOpen = SIZE_MAX; }
            else { role[i] = 1; inside = true; lastOpen = i; }
        }
        if (inside && lastOpen != SIZE_MAX) {
            if (n - lastOpen > kMaxOpenTail) role[lastOpen] = 0; else endQ = 1;
        }
    }
    // 곧은 작은따옴표: 문단 안에서 짝이 맞을 때만 (영문 don't 같은 줄임표는 제외)
    {
        std::vector<size_t> sq;
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != '\'') continue;
            bool apos = i > 0 && i + 1 < n && isAsciiAlpha(p[i - 1]) && isAsciiAlpha(p[i + 1]);
            if (!apos) sq.push_back(i);
        }
        if (sq.size() % 2 == 0)
            for (size_t k = 0; k + 1 < sq.size(); k += 2) { role[sq[k]] = 1; role[sq[k + 1]] = 2; }
    }
    // 둥근·낫표 따옴표, 괄호: 쌓아서 짝 맞춤
    std::vector<size_t> st;
    bool virt = startQ == 2;
    for (size_t i = 0; i < n; ++i) {
        C c = p[i];
        if (qCloseOf(c) || isBrOpen(c)) { st.push_back(i); continue; }
        if (isQClose(c) || isBrClose(c)) {
            if (!st.empty()) {
                C o = p[st.back()];
                C want = qCloseOf(o) ? qCloseOf(o) : brCloseOf(o);
                if (want == c) { role[st.back()] = 1; role[i] = 2; st.pop_back(); continue; }
                // 짝이 안 맞으면 안쪽 것을 버리고 다시 봄
                bool matched = false;
                for (size_t k = st.size(), lim = 0; k-- > 0 && lim < 8; ++lim) {
                    C o2 = p[st[k]];
                    if ((qCloseOf(o2) ? qCloseOf(o2) : brCloseOf(o2)) == c) { role[st[k]] = 1; role[i] = 2; st.resize(k); matched = true; break; }
                }
                if (matched) continue;
            }
            if (virt && isQClose(c) && st.empty()) { role[i] = 2; virt = false; }
        }
    }
    for (size_t k : st) {  // 닫히지 않은 것: 짧은 둥근 따옴표만 다음 문단으로 이어지는 대사로 봄
        if (qCloseOf(p[k]) && n - k <= kMaxOpenTail && k == st.front()) { role[k] = 1; if (!endQ) endQ = 2; }
    }
}

std::vector<Range> splitSentences(const S& p, int startQ = 0) {
    std::vector<Range> out;
    size_t n = p.size();
    std::vector<char> role; int endQ;
    encloseRoles(p, startQ, role, endQ);
    int depth = startQ ? 1 : 0;       // 따옴표·괄호 깊이
    bool encAtStart = startQ != 0;    // 가장 바깥 것이 조각 첫머리에서 열렸나
    bool encQuote = startQ != 0;      // 가장 바깥 것이 따옴표인가 (아니면 괄호)
    size_t start = 0;
    bool seenText = false;  // 지금 조각에 글자가 나왔나 (조각 첫머리 판단)
    auto cutAt = [&](size_t pos) {  // pos: 다음 조각 시작 위치
        if (pos <= start) return;
        size_t a = start, b = pos;
        while (a < b && isWS(p[a])) ++a;
        while (b > a && isWS(p[b - 1])) --b;
        if (b > a) out.push_back({a, b - a});
        start = pos;
        seenText = false;
    };
    auto nextNonWS = [&](size_t i) { while (i < n && isWS(p[i])) ++i; return i; };
    // 가장 바깥 따옴표·괄호가 닫힌 직후 (i = 닫는 글자 위치)
    // - 안에서 문장이 끝났고 바로 다음이 새 대사면 거기서 나눔: "안녕." "반가워."
    // - 대사 뒤에 이어지는 서술은 붙여 둠: "빨리 시작해!" 드렉셀은 고함을 질렀다.
    // - 문장 첫머리에서 연 괄호 속 문장이 끝났으면 나눔: (그는 떠났다.) 다음 날…
    auto afterClose = [&](size_t i) {
        C last = i > 0 ? p[i - 1] : 0;
        bool ended = isTermChar(last);
        size_t q = nextNonWS(i + 1);
        if (q >= n) return;
        if (ended && isOpenQuote(p[q]) && p[q] != '\'') { cutAt(q); return; }
        if (!encQuote && encAtStart && ended && q > i + 1 && !attachedAfterQuote(p, q)) cutAt(q);
    };

    auto isSepSym = [](C c) {
        return c == '*' || c == '-' || c == '=' || c == '#' || c == '~' || c == 0x25C6 || c == 0x25C7 || c == 0x25A0 ||
               c == 0x25A1 || c == 0x25CB || c == 0x25CF || c == 0x2605 || c == 0x2606 || c == 0x2500 || c == 0x2501 ||
               c == 0xFF0A || c == 0x203B;
    };
    for (size_t i = 0; i < n; ++i) {
        C c = p[i];
        if (depth == 0 && isSepSym(c) && (i == 0 || isWS(p[i - 1]))) {
            size_t j = i; int cnt = 0;
            while (j < n && (isSepSym(p[j]) || p[j] == ' ')) { if (p[j] != ' ') cnt++; ++j; }
            if (cnt >= 3 && (j >= n || isWS(p[j - 1]))) {
                cutAt(i);
                cutAt(j);
                i = j - 1;
                continue;
            }
        }
        if (role[i] == 1) {
            if (depth == 0) { encAtStart = !seenText; encQuote = !isBrOpen(c); }
            seenText = true;
            depth++;
            continue;
        }
        if (!isWS(c)) seenText = true;
        if (role[i] == 2) {
            if (depth > 0) { depth--; if (depth == 0) afterClose(i); }
            continue;
        }
        if (depth > 0) continue;
        if (!isTermChar(c)) continue;

        // 종결부호 묶음
        size_t j = i;
        while (j + 1 < n && isTermChar(p[j + 1])) ++j;
        bool strong = false;
        int dotCount = 0;
        for (size_t k = i; k <= j; ++k) {
            if (p[k] == '!' || p[k] == '?' || p[k] == 0x3002 || p[k] == 0xFF01 || p[k] == 0xFF1F) strong = true;
            if (p[k] == '.') dotCount++;
        }
        bool onlyOneDot = (j == i && c == '.');
        if (onlyOneDot) strong = true;
        C before = i > 0 ? p[i - 1] : 0;
        if (onlyOneDot && (isAbbrev(p, i) || (isDigit(before) && i + 1 < n && isDigit(p[i + 1])))) { i = j; continue; }
        if (onlyOneDot && isDigit(before)) {  // "1. 서론" "12. 그날" 같은 번호 매김
            size_t a = start; while (a < i && isWS(p[a])) ++a;
            bool num = a < i;
            for (size_t q = a; q < i; ++q) if (!isDigit(p[q])) { num = false; break; }
            if (num && i - a <= 3) { i = j; continue; }
            size_t q = i + 1; while (q < n && isWS(p[q])) ++q;
            if (q < n && isDigit(p[q])) { i = j; continue; }  // "1. 2. 3." 나열
        }
        bool endOk = strong || isEndingSyl(before);
        size_t after = j + 1;
        if (after >= n) { i = j; continue; }
        C nx = p[after];
        auto jamoTail = [&](size_t q) { return q < n && p[q] >= 0x3131 && p[q] <= 0x318E; };  // "뭐해? ㅋㅋ" "좋아!ㅎㅎ" 같은 웃음·울음 표시는 앞 문장에 붙임
        if (isWS(nx)) {
            if (endOk && !(strong && jamoTail(nextNonWS(after)))) cutAt(nextNonWS(after));
        } else if (isOpenQuote(nx) && nx != '\'') {
            cutAt(after);
        } else if (isHangulSyl(nx) && isHangulSyl(before) && (strong && (!onlyOneDot || isEndingSyl(before)))) {
            // 줄바꿈이 공백 없이 지워진 글: "했다.그는"
            cutAt(after);
        }
        i = j;
    }
    cutAt(n);
    return out;
}

bool isDialogue(const S& u) { return isOpenQuote(firstVis(u)); }

// 글 + 각 글자의 원본 위치
struct TM {
    S t;
    std::vector<int> m;
    void add(C c, int pos) { t += c; m.push_back(pos); }
    void add(const TM& o) { t += o.t; m.insert(m.end(), o.m.begin(), o.m.end()); }
    void add(const S& s, int pos) { for (C c : s) add(c, pos); }
    TM sub(size_t a, size_t len) const { TM r; r.t = t.substr(a, len); r.m.assign(m.begin() + a, m.begin() + a + len); return r; }
    int first() const { return m.empty() ? 0 : m[0]; }
};

// 보이지 않는 글자를 잠시 빼고 f로 다듬은 뒤, 원래 위치 순서대로 다시 끼움 (순서·개수 그대로 보존)
template <class F> TM visApply(const TM& x, F f) {
    bool any = false;
    for (C c : x.t) if (isInvisible(c)) { any = true; break; }
    if (!any) return f(x);
    TM v; std::vector<size_t> invIdx;
    for (size_t i = 0; i < x.t.size(); ++i) {
        if (isInvisible(x.t[i])) invIdx.push_back(i); else v.add(x.t[i], x.m[i]);
    }
    TM r = f(v), out;
    size_t q = 0;
    for (size_t j = 0; j < r.t.size(); ++j) {
        while (q < invIdx.size() && x.m[invIdx[q]] < r.m[j]) { out.add(x.t[invIdx[q]], x.m[invIdx[q]]); ++q; }
        out.add(r.t[j], r.m[j]);
    }
    for (; q < invIdx.size(); ++q) out.add(x.t[invIdx[q]], x.m[invIdx[q]]);
    return out;
}

TM collapseSpaces(const TM& s) {
    TM r; r.t.reserve(s.t.size()); r.m.reserve(s.m.size());
    bool prevSp = false;
    for (size_t i = 0; i < s.t.size(); ++i) {
        C c = s.t[i];
        if (c == ' ') { if (!prevSp) r.add(c, s.m[i]); prevSp = true; }
        else { r.add(c, s.m[i]); prevSp = false; }
    }
    return r;
}

bool isSpaceLike(C c) { return c == ' ' || c == 0x3000 || c == '\t'; }

// 글자마다 띄어 쓴 글("안 녕 하 세 요  반 가 워 요")인가: 한 글자 토큰이 대부분이고 진짜 띄어쓰기는 두 칸 이상
bool detectLetterSpaced(const S& t) {
    int tokens = 0, single = 0, multiRuns = 0;
    size_t i = 0, n = t.size();
    while (i < n) {
        while (i < n && isSpaceLike(t[i])) ++i;
        if (i >= n) break;
        size_t a = i;
        while (i < n && !isSpaceLike(t[i])) ++i;
        tokens++;
        if (i - a == 1 && isHangulSyl(t[a])) single++;
        size_t sp = i;
        while (i < n && isSpaceLike(t[i])) ++i;
        if (i < n && i - sp >= 2) multiRuns++;
    }
    return tokens >= 8 && single * 10 >= tokens * 7 && multiRuns >= 2;
}

// 공백·문장부호 다듬기
TM fixSpacing(const TM& in, int& fixes, bool& letterSpaced, int startQ = 0) {
    const S& t0 = in.t;
    size_t n0 = t0.size();
    // 1) 전각 공백·탭 -> 공백, 자간 띄운 글 복원, 연속 공백 1칸
    TM a; a.t.reserve(n0); a.m.reserve(n0);
    bool ls = detectLetterSpaced(t0);
    {   // 글자 사이를 크게 벌린 짧은 제목 한 줄: "경       고" "머  리  말" -> "경고" "머리말"
        int tokens = 0; bool ok = true, anyGap = false;
        for (size_t i = 0; i < n0 && ok;) {
            if (isSpaceLike(t0[i])) { ++i; continue; }
            if (!isHangulSyl(t0[i]) || (i + 1 < n0 && !isSpaceLike(t0[i + 1]))) { ok = false; break; }
            tokens++;
            size_t j = i + 1; while (j < n0 && isSpaceLike(t0[j])) ++j;
            if (j < n0) { if (j - (i + 1) < 2 && t0[i + 1] != 0x3000) ok = false; anyGap = true; }
            i = j;
        }
        if (ok && anyGap && tokens >= 2 && tokens <= 6) {
            TM r;
            for (size_t i = 0; i < n0; ++i) if (!isSpaceLike(t0[i])) r.add(t0[i], in.m[i]);
            fixes++;
            return r;
        }
    }
    if (ls) letterSpaced = true;
    for (size_t i = 0; i < n0;) {
        if (isSpaceLike(t0[i])) {
            size_t j = i;
            while (j < n0 && isSpaceLike(t0[j])) ++j;
            size_t run = j - i;
            if (ls && run == 1 && i > 0 && j < n0) { fixes++; i = j; continue; }  // 글자 사이 한 칸은 없앰
            if (run > 1 || t0[i] != ' ') fixes++;
            if (!a.t.empty()) a.add(U' ', in.m[i]);
            i = j;
            continue;
        }
        a.add(t0[i], in.m[i]);
        ++i;
    }
    while (!a.t.empty() && a.t.back() == ' ') { a.t.pop_back(); a.m.pop_back(); }
    // 2) ". . ." -> "..."
    {
        TM b; b.t.reserve(a.t.size()); b.m.reserve(a.m.size());
        for (size_t i = 0; i < a.t.size(); ++i) {
            if (a.t[i] == '.' && i + 4 < a.t.size() + 0 && a.t[i + 1] == ' ' && a.t[i + 2] == '.' && a.t[i + 3] == ' ' && a.t[i + 4] == '.') {
                size_t j = i;
                while (j + 2 < a.t.size() && a.t[j + 1] == ' ' && a.t[j + 2] == '.') { b.add(U'.', a.m[j]); j += 2; fixes++; }
                b.add(U'.', a.m[j]);
                i = j;
                continue;
            }
            b.add(a.t[i], a.m[i]);
        }
        a = std::move(b);
    }
    // 3) 따옴표 짝 (곧은 따옴표는 짝이 맞을 때만)
    const S& t = a.t;
    size_t n = t.size();
    int dq = 0;
    for (C c : t) if (c == '"') dq++;
    // 앞 문단에서 이어진 대사면 첫 따옴표가 닫는 것. 끝에서 열린 채 남는 것(다음 문단으로 이어지는 대사)도 허용
    bool dqOK = dq % 2 == 0 || startQ == 1;
    std::vector<char> dqOpen(n, 0);
    { bool open = startQ != 1; size_t last = SIZE_MAX;
      for (size_t i = 0; i < n; ++i) if (t[i] == '"') { dqOpen[i] = open; open = !open; last = i; }
      if (!dqOK && last != SIZE_MAX && dqOpen[last] && n - last <= kMaxOpenTail) dqOK = true; }
    auto isOpenQ = [&](size_t i) {
        C c = t[i];
        if (c == '"') return dqOK && dqOpen[i];
        return c == 0x201C || c == 0x2018 || c == 0x300C || c == 0x300E || c == '(' || c == '[' || c == 0x3010;
    };
    auto isCloseQ = [&](size_t i) {
        C c = t[i];
        if (c == '"') return dqOK && !dqOpen[i];
        return c == 0x201D || c == 0x300D || c == 0x300F || c == ')' || c == ']' || c == 0x3011;
    };
    auto isWordCh = [](C c) { return isLetterDigit(c); };
    // 4) 문장부호 주변
    TM r; r.t.reserve(n + n / 20); r.m.reserve(n + n / 20);
    for (size_t i = 0; i < n; ++i) {
        C c = t[i];
        if (c == ' ') {
            C prev = r.t.empty() ? 0 : r.t.back();
            size_t pi = i - 1;  // 원래 앞 글자 위치
            C next = i + 1 < n ? t[i + 1] : 0;
            // 문장부호 앞의 공백: "했다 ." "왜 ?" "그래 ," -> 붙임 (말줄임표 "그리고 ..."는 둠)
            bool nextPunct = (next == '.' || next == ',' || next == '!' || next == '?' || next == ';' || next == 0xFF0C || next == 0x3001);
            if (nextPunct && prev && (isWordCh(prev) || isCloseQuote(prev)) && !(next == '.' && i + 2 < n && t[i + 2] == '.')) { fixes++; continue; }
            // 문장 끝 말줄임표 앞 공백: "없었다 ..." "그래 …" (뒤가 끝이거나 공백일 때만)
            if ((next == '.' || next == 0x2026) && prev && isEndingSyl(prev)) {
                size_t j = i + 1; while (j < n && (t[j] == '.' || t[j] == 0x2026)) ++j;
                bool ell = (next == 0x2026) || j - (i + 1) >= 2;
                if (ell && (j >= n || t[j] == ' ' || isCloseQ(j))) { fixes++; continue; }
            }
            // 여는 따옴표/괄호 바로 뒤, 닫는 따옴표/괄호 바로 앞의 공백
            if (i > 0 && isOpenQ(pi)) { fixes++; continue; }
            if (i + 1 < n && isCloseQ(i + 1)) {
                // 글 속 인용의 문장부호 뒤 공백은 둠: 모든 글에서 "~다. " 뒤마다 (대사 "그래. " 는 붙임)
                bool keep = false;
                if (prev == '.') {
                    size_t o = i;
                    while (o > 0 && !isOpenQ(o - 1)) --o;
                    if (o > 0) {
                        size_t k = o - 1;
                        while (k > 0 && t[k - 1] == ' ') --k;
                        keep = k > 0 && !isTermChar(t[k - 1]) && !isCloseQuote(t[k - 1]);
                    }
                }
                if (!keep) { fixes++; continue; }
            }
            r.add(c, a.m[i]);
            continue;
        }
        r.add(c, a.m[i]);
        C prev = r.t.size() >= 2 ? r.t[r.t.size() - 2] : 0;  // 고친 뒤 기준의 앞 글자 ("했다 .그" -> "했다.그")
        C next = i + 1 < n ? t[i + 1] : 0;
        if (!next || next == ' ') continue;
        // 말줄임표 뒤 공백 누락: "않았다……그저" (말줄임표 앞이 문장 끝 어미일 때만)
        if ((c == '.' || c == 0x2026 || c == 0x2025) && isHangulSyl(next)) {
            size_t b = i + 1; int cnt = 0;
            while (b > 0 && (t[b - 1] == '.' || t[b - 1] == 0x2026 || t[b - 1] == 0x2025)) { --b; cnt += (t[b] == '.') ? 1 : 2; }
            if (cnt >= 2 && b > 0 && isEndingSyl(t[b - 1])) { r.add(U' ', a.m[i + 1]); fixes++; continue; }
        }
        // 쉼표 뒤 공백 누락: "그러나,그는" (숫자 1,000은 제외)
        if ((c == ',' || c == 0xFF0C) && prev && !isDigit(prev) && (isHangulSyl(next) || isAsciiAlpha(next)) && (isHangul(prev) || isAsciiAlpha(prev) || isCloseQuote(prev))) {
            r.add(U' ', a.m[i + 1]); fixes++; continue;
        }
        // 문장 끝 뒤 공백 누락: "했다.그는" "뭐?그래" (. 는 한글 어미 뒤만)
        if ((c == '.' || c == '!' || c == '?') && isHangulSyl(prev) && (c != '.' || isEndingSyl(prev))) {
            bool nextOpenCurly = next == 0x201C || next == 0x300C || next == 0x300E;
            if (isHangulSyl(next) || nextOpenCurly || (next == '"' && dqOK && dqOpen[i + 1])) { r.add(U' ', a.m[i + 1]); fixes++; continue; }
        }
    }
    while (!r.t.empty() && r.t.back() == ' ') { r.t.pop_back(); r.m.pop_back(); }
    return r;
}

// 끝의 공백 제거 (보이지 않는 글자는 남기고 그 앞뒤 공백만 지움)
void trimTailWS(TM& p) {
    size_t k = p.t.size();
    while (k > 0 && (isWS(p.t[k - 1]) || isInvisible(p.t[k - 1]))) --k;
    TM r = p.sub(0, k);
    for (size_t i = k; i < p.t.size(); ++i) if (isInvisible(p.t[i])) r.add(p.t[i], p.m[i]);
    p = r;
}
// 보이는 글자만 모은 글 + 원래 위치
void visProject(const S& t, S& v, std::vector<size_t>& idx) {
    v.clear(); idx.clear();
    for (size_t i = 0; i < t.size(); ++i) if (!isInvisible(t[i])) { v += t[i]; idx.push_back(i); }
}
// 한 줄로 이어진 글에서 "문장 끝 + 두 칸 이상 공백"이 문단 구분 역할을 하는 경우 나눔
// (모든 문장 뒤에 두 칸을 치는 습관이면 문단 구분이 아니므로 나누지 않음)
std::vector<TM> splitWideGaps(const TM& x, int& count) {
    std::vector<TM> out;
    S t; std::vector<size_t> idx;
    visProject(x.t, t, idx);  // 판단은 보이는 글자로
    size_t n = t.size();
    std::vector<size_t> cuts;  // 다음 문단 시작 위치
    int wide = 0, single = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        C c = t[i];
        if (!(isTermChar(c) || isCloseQuote(c))) continue;
        if (!isSpaceLike(t[i + 1])) continue;
        if (i + 1 < n && (isTermChar(t[i + 1]))) continue;
        size_t j = i + 1;
        bool full = false;
        while (j < n && isSpaceLike(t[j])) { if (t[j] == 0x3000) full = true; ++j; }
        if (j >= n) break;
        if (j - (i + 1) >= 2 || full) { wide++; cuts.push_back(j); } else single++;
    }
    if (wide < 2 || wide * 10 > (wide + single) * 6) { out.push_back(x); return out; }
    size_t start = 0;
    for (size_t c : cuts) { size_t oc = idx[c]; out.push_back(x.sub(start, oc - start)); start = oc; }
    out.push_back(x.sub(start, x.t.size() - start));
    count += (int)cuts.size();
    for (auto& p : out) trimTailWS(p);  // 끝 공백 정리
    return out;
}

// 쪽 번호 줄: "- 12 -", "12 / 300", "Page 12", "p.12", "[12]", "12쪽", 또는 숫자만
bool isPageNumLine(const S& t, bool& pureDigits) {
    pureDigits = false;
    if (t.empty() || t.size() > 20) return false;
    size_t i = 0, n = t.size();
    auto skipSp = [&]() { while (i < n && (t[i] == ' ' || t[i] == 0x3000)) ++i; };
    bool deco = false;
    skipSp();
    while (i < n && (t[i] == '-' || t[i] == 0x2013 || t[i] == 0x2014 || t[i] == '[' || t[i] == '(' || t[i] == '<' || t[i] == '=')) { ++i; deco = true; }
    skipSp();
    S low;
    for (size_t k = i; k < n; ++k) { C c = t[k]; if (c >= 'A' && c <= 'Z') c += 32; low += c; }
    size_t k = 0;
    if (low.compare(0, 4, U"page") == 0) { k = 4; deco = true; }
    else if (low.compare(0, 2, U"p.") == 0) { k = 2; deco = true; }
    i += k;
    skipSp();
    size_t d0 = i;
    while (i < n && isDigit(t[i])) ++i;
    size_t nd = i - d0;
    if (nd == 0 || nd > 4) return false;
    skipSp();
    if (i < n && t[i] == '/') {  // 12 / 300
        ++i; skipSp();
        size_t e0 = i;
        while (i < n && isDigit(t[i])) ++i;
        if (i == e0) return false;
        deco = true;
        skipSp();
    }
    if (i < n && (t[i] == 0xCABD /*쪽*/ || t[i] == 0xD398 /*페*/)) {
        deco = true;
        while (i < n && isHangulSyl(t[i])) ++i;
        skipSp();
    }
    while (i < n && (t[i] == '-' || t[i] == 0x2013 || t[i] == 0x2014 || t[i] == ']' || t[i] == ')' || t[i] == '>' || t[i] == '=')) { ++i; deco = true; }
    skipSp();
    if (i != n) return false;
    pureDigits = !deco;
    return true;
}

// HTML 찌꺼기: 알려진 개체/태그만 처리. 반환: 소비한 글자 수(0 = 해당 없음), 대체 글자(0 = 없음), newline = 줄바꿈 태그
size_t matchHtml(const S& in, size_t i, C& repl, bool& newline) {
    repl = 0; newline = false;
    size_t n = in.size();
    if (in[i] == '&') {
        struct E { const char32_t* name; C ch; };
        static const E ents[] = {{U"&nbsp;", ' '}, {U"&quot;", '"'}, {U"&amp;", '&'}, {U"&lt;", '<'}, {U"&gt;", '>'},
                                 {U"&apos;", '\''}, {U"&#39;", '\''}, {U"&hellip;", 0x2026}, {U"&middot;", 0xB7},
                                 {U"&lsquo;", 0x2018}, {U"&rsquo;", 0x2019}, {U"&ldquo;", 0x201C}, {U"&rdquo;", 0x201D},
                                 {U"&ndash;", 0x2013}, {U"&mdash;", 0x2014}, {nullptr, 0}};
        for (int k = 0; ents[k].name; ++k) if (startsWith(in, i, ents[k].name)) { repl = ents[k].ch; return std::char_traits<char32_t>::length(ents[k].name); }
        if (i + 3 < n && in[i + 1] == '#') {  // &#12345; / &#x1F600;
            size_t j = i + 2; bool hex = false; unsigned long v = 0;
            if (j < n && (in[j] == 'x' || in[j] == 'X')) { hex = true; ++j; }
            size_t d0 = j;
            while (j < n && j - d0 < 7) {
                C c = in[j];
                if (c >= '0' && c <= '9') v = v * (hex ? 16 : 10) + (c - '0');
                else if (hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) v = v * 16 + ((c | 0x20) - 'a' + 10);
                else break;
                ++j;
            }
            if (j > d0 && j < n && in[j] == ';' && v > 0 && v < 0x110000) { repl = (C)v; return j + 1 - i; }
        }
        return 0;
    }
    if (in[i] == '<') {
        size_t j = i + 1;
        if (j < n && in[j] == '/') ++j;
        size_t t0 = j;
        while (j < n && j - t0 < 8 && isAsciiAlpha(in[j])) ++j;
        if (j == t0) return 0;
        S name;
        for (size_t k = t0; k < j; ++k) name += (C)(in[k] | 0x20);
        static const char32_t* tags[] = {U"br", U"p", U"div", U"span", U"b", U"i", U"u", U"strong", U"em", U"font", U"hr", nullptr};
        bool known = false;
        for (int k = 0; tags[k]; ++k) if (name == tags[k]) { known = true; break; }
        if (!known) return 0;
        if (j < n && !(in[j] == '>' || in[j] == ' ' || in[j] == '/')) return 0;
        size_t e = j;
        while (e < n && e - j < 300 && in[e] != '>' && in[e] != '\n' && in[e] != '<') ++e;
        if (e >= n || in[e] != '>') return 0;
        newline = name == U"br" || name == U"p" || name == U"div" || name == U"hr";
        return e + 1 - i;
    }
    return 0;
}

// 지우는 줄에 들어 있던 보이지 않는 글자는 원문 보존을 위해 남은 줄로 옮김: 앞 줄 끝, 없으면 대기
template <class LineT>
void keepInvisible(const LineT& dead, std::vector<LineT>& kept, S& pend, std::vector<int>& pendM) {
    for (size_t k = 0; k < dead.t.size(); ++k) {
        if (!isInvisible(dead.t[k])) continue;
        if (!kept.empty()) { kept.back().t += dead.t[k]; kept.back().m.push_back(dead.m[k]); }
        else { pend += dead.t[k]; pendM.push_back(dead.m[k]); }
    }
}
S visText(const S& t) { S r; r.reserve(t.size()); for (C c : t) if (!isInvisible(c)) r += c; return r; }

template <class LineT>
void takePending(LineT& L, S& pend, std::vector<int>& pendM) {
    if (pend.empty()) return;
    L.t.insert(0, pend);
    L.m.insert(L.m.begin(), pendM.begin(), pendM.end());
    pend.clear(); pendM.clear();
}

bool isHan(C c) { return (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0xF900 && c <= 0xFAFF); }

// 따옴표·말줄임표·한자·전각 정리 (문단 단위)
TM charStyle(const TM& in, const TidyOptions& opt, int& fixes) {
    const S& t = in.t;
    size_t n = t.size();
    TM r; r.t.reserve(n); r.m.reserve(n);
    // 곧은/둥근 큰따옴표 짝
    int dq = 0, lc = 0, rc = 0;
    for (C c : t) { if (c == '"') dq++; else if (c == 0x201C) lc++; else if (c == 0x201D) rc++; }
    bool dqOK = dq % 2 == 0;
    bool openNext = true;
    for (size_t i = 0; i < n; ++i) {
        C c = t[i];
        int pos = in.m[i];
        // 괄호 속 한자
        if (opt.stripHanja && (c == '(' || c == 0xFF08)) {
            size_t j = i + 1; bool allHan = true;
            while (j < n && t[j] != ')' && t[j] != 0xFF09 && j - i <= 20) { if (!isHan(t[j]) && t[j] != ' ' && t[j] != 0xB7 && t[j] != ',') allHan = false; ++j; }
            if (j < n && j > i + 1 && allHan && (t[j] == ')' || t[j] == 0xFF09)) {
                // 앞이 한자 단어면 "學校(학교)" 꼴이 아니므로 그냥 괄호째 지움
                fixes++;
                i = j;
                continue;
            }
        }
        if (opt.stripHanja && isHan(c)) {
            // 學校(학교) -> 학교 : 한자 덩어리 뒤에 괄호 속 한글
            size_t j = i; while (j < n && isHan(t[j])) ++j;
            if (j < n && (t[j] == '(' || t[j] == 0xFF08)) {
                size_t k = j + 1; bool allHg = true;
                while (k < n && t[k] != ')' && t[k] != 0xFF09 && k - j <= 20) { if (!isHangul(t[k]) && t[k] != ' ') allHg = false; ++k; }
                if (k < n && k > j + 1 && allHg) {
                    for (size_t q = j + 1; q < k; ++q) r.add(t[q], in.m[q]);
                    fixes++;
                    i = k;
                    continue;
                }
            }
        }
        // 말줄임표
        if (opt.ellipsisMode && (c == '.' || c == 0x2026 || c == 0x2025 || c == 0x3002)) {
            size_t j = i; int dots = 0, ell = 0, other = 0;
            while (j < n && (t[j] == '.' || t[j] == 0x2026 || t[j] == 0x2025 || t[j] == 0x3002)) {
                if (t[j] == '.') dots++; else if (t[j] == 0x3002) other++; else ell++;
                ++j;
            }
            bool isEll = ell > 0 || dots >= 2 || other >= 2;
            if (isEll) {
                const char32_t* want = opt.ellipsisMode == 1 ? U"…" : opt.ellipsisMode == 2 ? U"……" : U"...";
                S old = t.substr(i, j - i);
                if (old != want) fixes++;
                for (const char32_t* w = want; *w; ++w) r.add(*w, pos);
                i = j - 1;
                continue;
            }
        }
        // 전각 -> 반각
        if (opt.fullToHalf && c >= 0xFF01 && c <= 0xFF5E) {
            r.add(c - 0xFEE0, pos); fixes++; continue;
        }
        // 따옴표
        if (opt.quoteMode == 2) {
            if (c == 0x201C || c == 0x201D || c == 0x201E || c == 0x00AB || c == 0x00BB) { r.add(U'"', pos); fixes++; continue; }
            if (c == 0x2018 || c == 0x2019 || c == 0x201A) { r.add(U'\'', pos); fixes++; continue; }
        } else if (opt.quoteMode == 1) {
            if (c == '"' && dqOK) { r.add(openNext ? (C)0x201C : (C)0x201D, pos); openNext = !openNext; fixes++; continue; }
            if (c == '\'') {
                C p = i > 0 ? t[i - 1] : 0, nx = i + 1 < n ? t[i + 1] : 0;
                if (!(isAsciiAlpha(p) && isAsciiAlpha(nx))) {  // don't 같은 영어 축약은 그대로
                    bool open = !p || p == ' ' || isOpenQuote(p) || p == '(';
                    r.add(open ? (C)0x2018 : (C)0x2019, pos); fixes++; continue;
                }
            }
        }
        r.add(c, pos);
    }
    (void)lc; (void)rc;
    return r;
}

// 사용자 치환 (문단 단위)
TM applyReplaces(const TM& in, const TidyOptions& opt, int& hits) {
    if (opt.replaces.empty()) return in;
    TM cur = in;
    for (auto& rp : opt.replaces) {
        const S& f = rp.first;
        if (f.empty()) continue;
        size_t p = cur.t.find(f);
        if (p == S::npos) continue;
        TM r; r.t.reserve(cur.t.size()); r.m.reserve(cur.m.size());
        size_t i = 0;
        while (p != S::npos) {
            for (size_t k = i; k < p; ++k) r.add(cur.t[k], cur.m[k]);
            for (C c : rp.second) r.add(c, cur.m[p]);
            hits++;
            i = p + f.size();
            p = cur.t.find(f, i);
        }
        for (size_t k = i; k < cur.t.size(); ++k) r.add(cur.t[k], cur.m[k]);
        // 치환으로 생긴 앞뒤·연속 공백 정리
        TM c2 = collapseSpaces(r);
        size_t a = 0; while (a < c2.t.size() && c2.t[a] == ' ') ++a;
        size_t b = c2.t.size(); while (b > a && c2.t[b - 1] == ' ') --b;
        cur = c2.sub(a, b - a);
    }
    return cur;
}

// 주소(URL)만 있는 줄: "www.abc.com", "출처: http://..."
bool isUrlLine(const S& t) {
    size_t p = S::npos;
    for (const char32_t* k : {U"http://", U"https://", U"www."}) {
        size_t q = t.find(k);
        if (q != S::npos && (p == S::npos || q < p)) p = q;
    }
    if (p == S::npos) return false;
    size_t e = p;
    while (e < t.size() && t[e] != ' ' && t[e] < 0x3000) ++e;  // 주소 끝
    int rest = 0;
    for (size_t i = 0; i < t.size(); ++i) if ((i < p || i >= e) && isLetterDigit(t[i])) rest++;
    return rest <= 8;
}

// 이어진 글 속 회차 제목 위치: "… 했다. 제2화 …", "… 끝. 3화 …", "Chapter 4"
bool chapterAt(const S& t, size_t i, size_t& len, bool mid = false) {
    size_t j = i, n = t.size();
    if (i >= n) return false;
    // 괄호로 감싼 회차 표시: [제1화] 【2화】 <3화> 〈외전〉 《프롤로그》
    {
        C o = t[i], want = 0;
        if (o == '[') want = ']'; else if (o == 0x3010) want = 0x3011; else if (o == '<') want = '>';
        else if (o == 0x3008) want = 0x3009; else if (o == 0x300A) want = 0x300B;  // ( )는 "(3장 참고)" 같은 참조가 흔해 제외
        if (want) {
            size_t k = i + 1;
            while (k < n && t[k] == ' ') ++k;
            size_t l2;
            if (k < n && t[k] != want && t[k] != '[' && chapterAt(t, k, l2)) {  // 괄호 안이면 "3장"처럼 제 없는 표시도 인정
                size_t e = k + l2;
                size_t lim = std::min(n, e + 4);
                while (e < lim && t[e] != want) ++e;
                if (e < n && t[e] == want) { len = e + 1 - i; return true; }
            }
            return false;
        }
    }
    bool je = false;
    if (t[j] == 0xC81C /*제*/) { je = true; ++j; if (j < n && t[j] == ' ') ++j; }
    size_t d0 = j;
    while (j < n && isDigit(t[j]) && j - d0 < 5) ++j;
    if (j > d0) {
        if (j < n && t[j] == ' ') ++j;
        bool unitOk = t[j] == 0xD654 /*화*/ || t[j] == 0xC7A5 /*장*/ || t[j] == 0xAD8C /*권*/ || t[j] == 0xBD80 /*부*/ ||
                      t[j] == 0xD68C /*회*/ || t[j] == 0xD3B8 /*편*/ || t[j] == 0xB9C9 /*막*/;
        // 글 중간에서는 "2장 분량의", "3권을" 같은 수량 표현과 헷갈리므로 "제"가 없으면 "N화"만 회차로 봄
        if (mid && !je && j < n && t[j] != 0xD654) unitOk = false;
        if (j < n && unitOk) {
            ++j;
            if (j >= n || t[j] == ' ' || t[j] == '.' || t[j] == ':' || t[j] == ')' || t[j] == ']' || t[j] == 0x3011 || t[j] == '>' ||
                t[j] == 0x3009 || t[j] == 0x300B) { len = j - i; return true; }
        }
        return false;
    }
    if (je) return false;
    if (startsWithCI(t.substr(i, 8), "chapter ")) { len = 8; while (i + len < n && isDigit(t[i + len])) ++len; return len > 8; }
    for (const char32_t* k : {U"프롤로그", U"에필로그", U"외전"}) {
        size_t kl = std::char_traits<char32_t>::length(k);
        if (t.compare(i, kl, k) == 0) {
            C nx = i + kl < n ? t[i + kl] : ' ';
            if (k[0] != 0xC678 /*외*/ || nx == ' ' || isDigit(nx) || nx == ']' || nx == '>' || nx == ')' || nx == 0x3011 || nx == 0x3009 || nx == 0x300B) {
                len = kl; return true;
            }
        }
    }
    // 덧붙임 글: "PS:", "P.S.", "추신:", "작가의 말", "후기:"
    {
        S head = t.substr(i, 12);
        for (auto& c : head) if (c >= 'a' && c <= 'z') c -= 32;
        for (const char32_t* k : {U"PS:", U"PS.", U"P.S", U"PS ", U"추신:", U"추신 :", U"작가의 말", U"작가의말", U"후기:"}) {
            size_t kl = std::char_traits<char32_t>::length(k);
            if (head.compare(0, kl, k) == 0) { len = kl; return true; }
        }
    }
    return false;
}
std::vector<TM> splitChapters(const TM& x, int& count) {
    std::vector<TM> out;
    S t; std::vector<size_t> idx;
    visProject(x.t, t, idx);  // 판단은 보이는 글자로
    size_t n = t.size(), start = 0;
    for (size_t i = 1; i + 2 < n; ++i) {
        if (!(t[i - 1] == ' ' && i >= 2 && (isTermChar(t[i - 2]) || isCloseQuote(t[i - 2])))) continue;
        size_t len;
        if (!chapterAt(t, i, len, true)) continue;
        size_t oi = idx[i];
        out.push_back(x.sub(start, oi - start));
        start = oi;
        count++;
    }
    out.push_back(x.sub(start, x.t.size() - start));
    for (auto& p : out) trimTailWS(p);
    return out;
}

// 문장 조각 나누기: 보이는 글자로 판단하고, 조각 사이의 보이지 않는 글자는 앞 조각 끝에 붙여 보존
std::vector<TM> splitUnits(const TM& x, int startQ = 0) {
    S v; std::vector<size_t> idx;
    visProject(x.t, v, idx);
    std::vector<Range> rs = splitSentences(v, startQ);
    std::vector<TM> units;
    if (rs.empty()) { units.push_back(x); return units; }
    size_t prevEnd = 0;
    for (size_t k = 0; k < rs.size(); ++k) {
        size_t a = idx[rs[k].a], b = idx[rs[k].a + rs[k].len - 1] + 1;
        if (k == 0) {
            TM u;
            for (size_t i = 0; i < a; ++i) if (isInvisible(x.t[i])) u.add(x.t[i], x.m[i]);
            u.add(x.sub(a, b - a));
            units.push_back(u);
        } else {
            for (size_t i = prevEnd; i < a; ++i) if (isInvisible(x.t[i])) units.back().add(x.t[i], x.m[i]);
            units.push_back(x.sub(a, b - a));
        }
        prevEnd = b;
    }
    for (size_t i = prevEnd; i < x.t.size(); ++i) if (isInvisible(x.t[i])) units.back().add(x.t[i], x.m[i]);
    return units;
}

// 회차 제목 뒤에 본문이 이어진 문단: "제1화 비 오는 밤 비가 내렸다. …" -> 제목 / 본문
// 제목 끝은 모르므로, 본문 첫 문장의 주어(…이/가/은/는/께서)가 나오기 직전까지를 제목으로 봄
bool splitTitleHead(const TM& x, TM& title, TM& rest) {
    S v; std::vector<size_t> idx;
    visProject(x.t, v, idx);
    size_t i = 0;
    while (i < v.size() && isWS(v[i])) ++i;
    size_t mlen;
    if (i >= v.size() || !chapterAt(v, i, mlen, true)) return false;  // "2장 분량의…" 같은 수량 표현은 제외
    S head = v.substr(i, std::min<size_t>(mlen, 4));
    if (head[0] == 'P' || head[0] == 'p' || head[0] == 0xCD94 /*추*/ || head[0] == 0xC791 /*작*/ || head[0] == 0xD6C4 /*후*/) return false;  // 덧붙임 글
    size_t j = i + mlen;
    // 첫 문장 끝
    size_t end = j;
    while (end < v.size() && !isTermChar(v[end])) ++end;
    if (end >= v.size()) return false;
    // 제목 다음 낱말들을 살펴 본문이 시작되는 낱말을 찾음
    //  1) "그는", "그날" 같은 문장 첫머리 낱말
    //  2) 주격(…이/가/께서) 낱말  3) 목적격·부사격(…을/를/에게/에서/으로) 낱말  4) 주제(…은/는) 낱말
    //  단, 2~4는 그 앞(제목이 될 부분)에 "뜨자", "하고"처럼 문장을 잇는 말이나 목적어가 없을 때만
    //  (예: "[제1화] 각성 눈을 뜨자 낯선 천장이…" -> 제목 "각성", 본문 "눈을 뜨자…")
    struct Wd { size_t ws; int kind; bool conn; };  // kind: 0 보통, 1 첫머리, 2 주격, 3 목적·부사격, 4 주제
    std::vector<Wd> wds;
    size_t k = j;
    static const char32_t* startW[] = {U"그는", U"그가", U"그녀는", U"그녀가", U"나는", U"내가", U"우리는", U"우리가", U"그들은",
                                       U"그들이", U"그때", U"그날", U"그러나", U"하지만", U"그리고", U"그러자", U"이제", U"다시",
                                       U"한편", U"어느", U"갑자기", U"문득", U"마침내", U"결국", nullptr};
    while (k < end && wds.size() < 12) {
        while (k < end && isWS(v[k])) ++k;
        if (k >= end) break;
        size_t ws = k;
        while (k < end && !isWS(v[k])) ++k;
        size_t wl = k - ws;
        C last = v[k - 1], prev = wl >= 2 ? v[k - 2] : 0;
        Wd d{ws, 0, false};
        if (wl >= 2 && isHangulSyl(v[ws])) {
            for (int q = 0; startW[q]; ++q) {
                size_t ql = std::char_traits<char32_t>::length(startW[q]);
                if (wl == ql && v.compare(ws, ql, startW[q]) == 0) { d.kind = 1; break; }
            }
            if (!d.kind) {
                if (last == 0xC774 /*이*/ || last == 0xAC00 /*가*/ || (prev == 0xAED8 && last == 0xC11C) /*께서*/) d.kind = 2;
                else if (last == 0xC744 /*을*/ || last == 0xB97C /*를*/ || last == 0xC5D0 /*에*/ || (prev == 0xC5D0 && (last == 0xAC8C || last == 0xC11C)) /*에게 에서*/ ||
                         (prev == 0xC73C && last == 0xB85C) /*으로*/) d.kind = 3;
                else if (last == 0xC740 /*은*/ || last == 0xB294 /*는*/) d.kind = 4;
            }
            // 문장을 잇는 말: …자 …고 …며 …면 …니 …서 …여 …는데 …지만 …면서
            d.conn = last == 0xC790 || last == 0xACE0 || last == 0xBA70 || last == 0xBA74 || last == 0xB2C8 || last == 0xC11C || last == 0xC5EC ||
                     (prev == 0xB294 && last == 0xB370) || (prev == 0xC9C0 && last == 0xB9CC);
        }
        wds.push_back(d);
    }
    size_t ws = SIZE_MAX;
    int w = 0;
    for (size_t q = 0; q < wds.size() && ws == SIZE_MAX; ++q) if (wds[q].kind == 1) { ws = wds[q].ws; w = (int)q; }
    for (int want = 2; want <= 4 && ws == SIZE_MAX; ++want) {
        bool dirty = false;  // 앞에 이어 주는 말이나 목적어가 있었나
        for (size_t q = 0; q < wds.size(); ++q) {
            if (wds[q].kind == want && !dirty) { ws = wds[q].ws; w = (int)q; break; }
            if (wds[q].conn || wds[q].kind == 3) dirty = true;
        }
    }
    if (ws == SIZE_MAX || w > 6) return false;
    size_t cut = idx[ws];  // 원래 글에서 본문 시작 위치
    title = x.sub(0, cut);
    rest = x.sub(cut, x.t.size() - cut);
    trimTailWS(title);
    return !visText(title.t).empty() && !rest.t.empty();
}


// ---------- 정규식 ----------
// u32 <-> wstring (Windows의 wchar_t는 16비트라 보조평면 글자는 두 칸). idx: wstring 각 칸이 원래 몇 번째 글자인지
std::wstring toW(const S& t, std::vector<int>* idx) {
    std::wstring w; w.reserve(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        C c = t[i];
        if (sizeof(wchar_t) == 2 && c >= 0x10000) {
            c -= 0x10000;
            w += (wchar_t)(0xD800 + (c >> 10)); w += (wchar_t)(0xDC00 + (c & 0x3FF));
            if (idx) { idx->push_back((int)i); idx->push_back((int)i); }
        } else { w += (wchar_t)c; if (idx) idx->push_back((int)i); }
    }
    return w;
}
S fromW(const std::wstring& w) {
    S r; r.reserve(w.size());
    for (size_t i = 0; i < w.size(); ++i) {
        unsigned c = (unsigned)w[i];
        if (sizeof(wchar_t) == 2 && c >= 0xD800 && c < 0xDC00 && i + 1 < w.size() && (unsigned)w[i + 1] >= 0xDC00 && (unsigned)w[i + 1] < 0xE000) {
            r += (C)(0x10000 + ((c - 0xD800) << 10) + ((unsigned)w[i + 1] - 0xDC00)); ++i;
        } else r += (C)c;
    }
    return r;
}
struct CompiledRegex { std::wregex re; std::wstring rep; };
std::vector<CompiledRegex> compileRegex(const std::vector<std::pair<std::wstring, std::wstring>>& rules) {
    std::vector<CompiledRegex> v;
    for (auto& r : rules) {
        try { v.push_back({std::wregex(r.first, std::regex::ECMAScript), r.second}); } catch (...) {}
    }
    return v;
}
static const size_t kRegexChunk = 2000;  // 긴 글은 이 길이 안팎으로 나눠 적용 (정규식 엔진의 깊은 되풀이 방지)
// 보이는 글자만 있는 TM에 정규식 바꾸기
TM applyRegexTM(const TM& in, const std::vector<CompiledRegex>& rx, int& hits) {
    TM cur = in;
    for (auto& r : rx) {
        TM out; out.t.reserve(cur.t.size()); out.m.reserve(cur.m.size());
        size_t pos = 0, n = cur.t.size();
        bool changed = false;
        while (pos < n) {
            size_t end = n;
            if (n - pos > kRegexChunk) {  // 공백에서 자름
                end = pos + kRegexChunk;
                size_t lim = std::min(n, pos + kRegexChunk + 500);
                while (end < lim && cur.t[end] != ' ') ++end;
            }
            TM piece = cur.sub(pos, end - pos);
            std::vector<int> idx;
            std::wstring w = toW(piece.t, &idx);
            try {
                auto it = std::wsregex_iterator(w.begin(), w.end(), r.re), last = std::wsregex_iterator();
                size_t prev = 0;
                bool any = false;
                for (; it != last; ++it) {
                    const std::wsmatch& m = *it;
                    size_t ms = (size_t)m.position(0), ml = (size_t)m.length(0);
                    if (ml == 0 && m.format(r.rep).empty()) continue;
                    any = true;
                    // 앞부분 그대로
                    for (size_t k = prev; k < ms; ++k) if (k == 0 || idx[k] != idx[k - 1]) out.add(piece.t[idx[k]], piece.m[idx[k]]);
                    S repl = fromW(m.format(r.rep));
                    // 바뀐 글자의 원래 위치: 맞은 부분 안에서 차례로 대응 (넘치면 마지막 글자)
                    int at = ms < idx.size() ? piece.m[idx[ms]] : (piece.m.empty() ? 0 : piece.m.back());
                    size_t a0 = ms < idx.size() ? (size_t)idx[ms] : piece.t.size();
                    size_t a1 = (ms + ml) <= idx.size() && ml > 0 ? (size_t)idx[ms + ml - 1] : a0;
                    for (size_t q = 0; q < repl.size(); ++q) {
                        size_t src = std::min(a0 + q, a1);
                        out.add(repl[q], src < piece.m.size() ? piece.m[src] : at);
                    }
                    hits++;
                    prev = ms + ml;
                }
                if (!any) { out.add(piece); pos = end; continue; }
                changed = true;
                for (size_t k = prev; k < w.size(); ++k) if (k == 0 || idx[k] != idx[k - 1]) out.add(piece.t[idx[k]], piece.m[idx[k]]);
            } catch (...) {
                out.add(piece);
            }
            pos = end;
        }
        if (changed) {
            TM c2 = collapseSpaces(out);
            size_t a = 0; while (a < c2.t.size() && c2.t[a] == ' ') ++a;
            size_t b = c2.t.size(); while (b > a && c2.t[b - 1] == ' ') --b;
            cur = c2.sub(a, b - a);
        }
    }
    return cur;
}

// ---------- 문장마다 나눌 때 따로 띄울 줄 ----------
// 시스템 메시지·알림처럼 대괄호로 감싼 줄: [레벨이 올랐습니다.] 【퀘스트 완료】
bool isBracketLine(const S& t) {
    S v = visText(t);
    size_t a = 0, b = v.size();
    while (a < b && isWS(v[a])) ++a;
    while (b > a && (isWS(v[b - 1]) || v[b - 1] == '.' || v[b - 1] == '!' || v[b - 1] == '?')) --b;
    if (b - a < 3) return false;
    C o = v[a], c = v[b - 1];
    return (o == '[' && c == ']') || (o == 0x3010 && c == 0x3011) || (o == 0x3014 && c == 0x3015) || (o == 0xFF3B && c == 0xFF3D);
}
// 짧은 소리·외침 한 마디: "쾅!" "콰앙!!" "쿵!"
bool isSoundLine(const S& t) {
    S v = visText(t);
    size_t a = 0, b = v.size();
    while (a < b && isWS(v[a])) ++a;
    while (b > a && isWS(v[b - 1])) --b;
    size_t e = b;
    while (e > a && (v[e - 1] == '!' || v[e - 1] == '?' || v[e - 1] == '~')) --e;
    if (e == b || e == a || e - a > 3) return false;
    static const char32_t* notSfx[] = {U"뭐", U"왜", U"네", U"예", U"응", U"아니", U"그래", U"좋아", U"싫어", U"제발", U"잠깐", U"설마",
                                       U"정말", U"진짜", U"어서", U"빨리", U"그만", U"멈춰", U"가자", U"살려", U"아니야", U"안녕", nullptr};
    for (int q = 0; notSfx[q]; ++q) if (v.compare(a, e - a, notSfx[q]) == 0) return false;
    bool bang = false;
    for (size_t k = e; k < b; ++k) if (v[k] == '!') bang = true;
    if (!bang) return false;
    for (size_t k = a; k < e; ++k) if (!isHangulSyl(v[k])) return false;
    return true;
}
// 장면·시간이 바뀌는 첫머리: "한편," "다음 날" "그날 밤" "며칠 후" …
bool startsTransition(const S& t) {
    S v = visText(t);
    size_t a = 0; while (a < v.size() && isWS(v[a])) ++a;
    static const char32_t* kw[] = {U"한편", U"다음 날", U"다음날", U"이튿날", U"그날 밤", U"그날 저녁", U"그날 아침", U"그날 오후",
                                   U"며칠 후", U"며칠 뒤", U"몇 시간 후", U"몇 시간 뒤", U"몇 년 후", U"몇 년 뒤", U"수년 후", U"수년 뒤",
                                   U"얼마 후", U"얼마 뒤", U"그 무렵", U"같은 시각", U"그 시각", U"그로부터",
                                   U"시간이 흘러", U"세월이 흘러", nullptr};
    for (int i = 0; kw[i]; ++i) {
        size_t kl = std::char_traits<char32_t>::length(kw[i]);
        if (v.compare(a, kl, kw[i]) == 0) {
            C nx = a + kl < v.size() ? v[a + kl] : ' ';
            if (nx == ' ' || nx == ',' || nx == 0xFF0C) return true;
        }
    }
    return false;
}

// ---------- 긴 대사 ----------
// 따옴표 안의 대사가 길면(150자 이상이면서 세 문장 이상, 또는 100자 이상이면서 다섯 문장 이상)
// 대사 안에서도 문장마다 줄을 바꿈. 짧은 대사는 한 줄로 둠.
// 빈 줄 없이 줄만 바꿔서 한 대사로 보이게 하고, 여는·닫는 따옴표는 첫 줄·끝 줄에 그대로 둠
static const size_t kLongSpeech = 200;  // 이보다 짧은 대사는 한 줄로 둔다
std::vector<TM> splitLongSpeech(const TM& x, int startQ) {
    std::vector<TM> out;
    S v; std::vector<size_t> idx;
    visProject(x.t, v, idx);
    if (v.size() < kLongSpeech) { out.push_back(x); return out; }
    std::vector<char> role; int endQ = 0;
    encloseRoles(v, startQ, role, endQ);
    size_t a = 0, b = v.size();
    if (startQ) {  // 앞 문단에서 열린 대사가 이어지는 문단
        for (size_t i = 0; i < v.size(); ++i) if (role[i] == 2) { b = i; break; }
    } else {
        size_t i = 0;
        while (i < v.size() && isWS(v[i])) ++i;
        if (i >= v.size() || role[i] != 1 || isBrOpen(v[i])) { out.push_back(x); return out; }  // 따옴표로 시작하는 대사만
        int d = 0;
        size_t k = i;
        for (; k < v.size(); ++k) {
            if (role[k] == 1) d++;
            else if (role[k] == 2 && --d == 0) break;
        }
        a = i + 1; b = k;
    }
    if (b <= a || b - a < kLongSpeech) { out.push_back(x); return out; }
    std::vector<Range> rs = splitSentences(v.substr(a, b - a));
    size_t len = b - a;
    if (!(len >= kLongSpeech && rs.size() >= 3)) { out.push_back(x); return out; }
    size_t prev = 0;
    for (size_t k = 1; k < rs.size(); ++k) {
        size_t co = idx[a + rs[k].a];
        TM piece = x.sub(prev, co - prev);
        trimTailWS(piece);
        out.push_back(piece);
        prev = co;
    }
    TM last = x.sub(prev, x.t.size() - prev);
    trimTailWS(last);
    out.push_back(last);
    // 말줄임표로 끝난 조각은 다음 조각과 이어 둠
    std::vector<TM> merged;
    for (auto& t : out) {
        if (!merged.empty()) {
            const S& pt = merged.back().t;
            size_t k = pt.size();
            while (k > 0 && (isWS(pt[k - 1]) || isInvisible(pt[k - 1]))) --k;
            bool ell = k > 0 && (pt[k - 1] == 0x2026 || pt[k - 1] == 0x2025 || (pt[k - 1] == '.' && k >= 2 && pt[k - 2] == '.'));
            if (ell) { merged.back().add(U' ', t.first()); merged.back().add(t); continue; }
        }
        merged.push_back(t);
    }
    return merged;
}
}  // namespace

static std::u32string TidyCore(const std::u32string& in, const TidyOptions& opt, TidyReport* rep, std::vector<int>* outMap) {
    TidyReport R;
    // 1) 정규화 + 줄 나누기
    std::vector<Line> lines;
    {
        S cur;
        std::vector<int> curM;
        int pendingGap = 0;
        S pendInv; std::vector<int> pendInvM;  // 빈 줄로 처리한 줄에 있던 보이지 않는 글자 (원문 보존)
        auto flush = [&]() {
            R.inLines++;
            // 줄 앞뒤 공백 정리: 공백 사이에 낀 보이지 않는 글자는 남기고 공백만 지움
            {
                size_t n = cur.size(), a0 = 0, b0 = n;
                while (a0 < n && (isWS(cur[a0]) || isInvisible(cur[a0]))) ++a0;
                while (b0 > a0 && (isWS(cur[b0 - 1]) || isInvisible(cur[b0 - 1]))) --b0;
                bool wsIn = false;
                for (size_t k = 0; k < a0 && !wsIn; ++k) if (isInvisible(cur[k])) wsIn = true;
                for (size_t k = b0; k < n && !wsIn; ++k) if (isInvisible(cur[k])) wsIn = true;
                if (wsIn && a0 < b0) {  // 앞쪽 보이지 않는 글자는 공백 뒤로, 뒤쪽은 공백 앞으로 모음
                    S t2; std::vector<int> m2;
                    for (size_t k = 0; k < a0; ++k) if (isWS(cur[k])) { t2 += cur[k]; m2.push_back(curM[k]); }
                    for (size_t k = 0; k < a0; ++k) if (isInvisible(cur[k])) { t2 += cur[k]; m2.push_back(curM[k]); }
                    for (size_t k = a0; k < b0; ++k) { t2 += cur[k]; m2.push_back(curM[k]); }
                    for (size_t k = b0; k < n; ++k) if (isInvisible(cur[k])) { t2 += cur[k]; m2.push_back(curM[k]); }
                    for (size_t k = b0; k < n; ++k) if (isWS(cur[k])) { t2 += cur[k]; m2.push_back(curM[k]); }
                    cur.swap(t2); curM.swap(m2);
                }
            }
            size_t a = 0, b = cur.size();
            int lead = 0;
            while (a < b && isWS(cur[a])) { lead += (cur[a] == '\t') ? 4 : cw(cur[a]); ++a; }
            while (b > a && isWS(cur[b - 1])) --b;
            bool onlyInvisible = true;  // 공백·보이지 않는 글자뿐인 줄은 화면상 빈 줄
            for (size_t k = a; k < b; ++k) if (!isWS(cur[k]) && !isInvisible(cur[k])) { onlyInvisible = false; break; }
            if (a == b || onlyInvisible) {
                for (size_t k = a; k < b; ++k) if (isInvisible(cur[k])) {
                    if (!lines.empty()) { lines.back().t += cur[k]; lines.back().m.push_back(curM[k]); }
                    else { pendInv += cur[k]; pendInvM.push_back(curM[k]); }
                }
                pendingGap++; cur.clear(); curM.clear(); return;
            }
            Line L;
            L.t.reserve(b - a);
            for (size_t k = a; k < b; ++k) { C c = cur[k]; L.t += (isWS(c) && c != 0x3000) ? U' ' : c; }
            L.m.assign(curM.begin() + a, curM.begin() + b);
            if (!pendInv.empty()) {  // 글 맨 앞의 빈 줄에 있던 보이지 않는 글자
                L.t.insert(0, pendInv);
                L.m.insert(L.m.begin(), pendInvM.begin(), pendInvM.end());
                pendInv.clear(); pendInvM.clear();
            }
            L.lead = lead;
            L.trail = b < cur.size();
            L.gap = pendingGap;
            pendingGap = 0;
            L.w = widthOf(L.t);
            L.rw = L.w + lead + (L.trail ? 1 : 0);
            R.longestLine = std::max(R.longestLine, (int)L.t.size());
            lines.push_back(std::move(L));
            cur.clear();
            curM.clear();
        };
        // HTML 찌꺼기는 실제로 여러 번 나올 때만 처리 (본문에 우연히 "&lt;" 같은 글이 있는 경우 보호)
        bool doHtml = false;
        if (opt.removeJunk) {
            int hits = 0;
            for (size_t i = 0; i < in.size() && hits < 3; ++i) {
                if (in[i] == '&' || in[i] == '<') { C rp; bool nl; if (matchHtml(in, i, rp, nl)) hits++; }
            }
            doHtml = hits >= 3;
        }
        for (size_t i = 0; i < in.size(); ++i) {
            C c = in[i];
            if (c == '\r') { if (i + 1 < in.size() && in[i + 1] == '\n') ++i; flush(); continue; }
            if (c == '\n' || c == 0x2028 || c == 0x2029 || c == 0x85 || c == 0x0C) { flush(); continue; }
            if (doHtml && (c == '&' || c == '<')) {
                C rp; bool nl;
                size_t len = matchHtml(in, i, rp, nl);
                if (len) {
                    R.htmlFixes++;
                    if (nl) flush();
                    else if (rp) { cur += rp; curM.push_back((int)i); }
                    i += len - 1;
                    continue;
                }
            }
            if (c < 0x20 && c != '\t') continue;
            // 자모가 분리된 한글(맥에서 만든 파일 등): ᄒ+ᅡ+ᆫ -> 한
            if (c >= 0x1100 && c <= 0x1112 && i + 1 < in.size() && in[i + 1] >= 0x1161 && in[i + 1] <= 0x1175) {
                C syl = 0xAC00 + ((c - 0x1100) * 21 + (in[i + 1] - 0x1161)) * 28;
                size_t used = 2;
                if (i + 2 < in.size() && in[i + 2] >= 0x11A8 && in[i + 2] <= 0x11C2) { syl += in[i + 2] - 0x11A7; used = 3; }
                cur += syl; curM.push_back((int)i);
                R.jamoFixes++;
                i += used - 1;
                continue;
            }
            cur += c;
            curM.push_back((int)i);
        }
        if (!cur.empty()) flush();
        if (!pendInv.empty() && lines.empty()) {  // 글 전체가 보이지 않는 글자뿐
            Line L; L.t = pendInv; L.m = pendInvM; lines.push_back(std::move(L));
        }
    }
    // 지정 문구 / 주소만 있는 줄 / 반복되는 머리글 줄
    std::vector<std::wregex> delRx;
    for (auto& d : opt.regexDeleteLines) { try { delRx.emplace_back(d, std::regex::ECMAScript); } catch (...) {} }
    if (!opt.deleteLines.empty() || opt.removeJunk || !delRx.empty()) {
        std::map<S, int> freq;
        S invP; std::vector<int> invPm;
        if (opt.removeJunk) for (auto& L : lines) { S v = visText(L.t); if (v.size() >= 6 && v.size() <= 60) freq[v]++; }
        std::vector<Line> kept;
        kept.reserve(lines.size());
        int carryGap = -1;
        for (auto& L : lines) {
            bool kill = false;
            S vt = visText(L.t);  // 보이는 글자만으로 판단
            for (auto& d : opt.deleteLines) if (!d.empty() && vt.find(d) != S::npos) { kill = true; break; }
            if (!kill && !delRx.empty() && vt.size() <= kRegexChunk * 4) {
                std::wstring w = toW(vt, nullptr);
                for (auto& re : delRx) { try { if (std::regex_search(w, re)) { kill = true; break; } } catch (...) {} }
            }
            if (!kill && opt.removeJunk) {
                if (isUrlLine(vt)) kill = true;
                else if (vt.size() >= 6 && vt.size() <= 60) {
                    int f = freq[vt];
                    bool letters = false;
                    for (C c : vt) if (isLetterDigit(c)) { letters = true; break; }
                    if (f >= 8 && (size_t)f * 200 >= lines.size() && letters && !isOpenQuote(vt[0]) && endKind(vt) == 0) kill = true;
                }
            }
            if (kill) { R.junkLines++; if (carryGap < 0) carryGap = L.gap; keepInvisible(L, kept, invP, invPm); continue; }
            takePending(L, invP, invPm);
            if (carryGap >= 0) {
                bool prevOpen = !kept.empty() && endKind(kept.back().t) == 0;
                L.gap = prevOpen ? 0 : std::max(carryGap, L.gap);
                L.afterJunk = true;
                carryGap = -1;
            }
            kept.push_back(std::move(L));
        }
        if (!invP.empty()) { Line L; L.t = invP; L.m = invPm; kept.push_back(std::move(L)); }  // 모든 줄이 지워진 경우
        lines.swap(kept);
    }
    if (opt.removeJunk && lines.size() >= 3) {
        int deco = 0, pure = 0;
        std::vector<char> pn(lines.size(), 0);
        for (size_t i = 0; i < lines.size(); ++i) {
            bool pd;
            if (isPageNumLine(visText(lines[i].t), pd)) { pn[i] = pd ? 1 : 2; if (pd) pure++; else deco++; }
        }
        bool killDeco = deco >= 3;
        bool killPure = pure >= 10 && (size_t)pure * 80 >= lines.size();
        if (killDeco || killPure) {
            std::vector<Line> kept;
            kept.reserve(lines.size());
            int carryGap = -1;
            S invP; std::vector<int> invPm;
            for (size_t i = 0; i < lines.size(); ++i) {
                if ((pn[i] == 2 && killDeco) || (pn[i] == 1 && killPure)) {
                    R.pageNumLines++;
                    keepInvisible(lines[i], kept, invP, invPm);
                    if (carryGap < 0) carryGap = lines[i].gap;
                    continue;
                }
                Line L = std::move(lines[i]);
                takePending(L, invP, invPm);
                if (carryGap >= 0) {
                    // 쪽이 넘어가며 끊긴 문장이면 이어 붙을 수 있게 빈 줄을 없앰
                    bool prevOpen = !kept.empty() && endKind(kept.back().t) == 0;
                    L.gap = prevOpen ? 0 : std::max(carryGap, L.gap);
                    L.afterJunk = true;
                    carryGap = -1;
                }
                kept.push_back(std::move(L));
            }
            if (!invP.empty()) { Line L; L.t = invP; L.m = invPm; kept.push_back(std::move(L)); }
            lines.swap(kept);
        }
    }
    R.textLines = (int)lines.size();
    if (lines.empty()) { if (rep) *rep = R; if (outMap) outMap->clear(); return S(); }
    const int n = (int)lines.size();
    for (auto& L : lines) { L.sym = isSymbolLine(L.t); L.head = !L.sym && isHeading(L.t, L.w); }

    // 목록·기록형 글 (접속 기록, 표처럼 칸을 나눈 줄, 같은 말로 시작하는 줄이 이어지는 목록):
    // 한 줄이 한 항목이므로 줄을 합치거나 문장마다 나누지 않고 원문 줄 그대로 둠
    bool listLike = false;
    if (n >= 3) {
        auto firstTok = [&](const S& t) {
            S v = visText(t); size_t a = 0;
            while (a < v.size() && isWS(v[a])) ++a;
            size_t b = a; while (b < v.size() && !isWS(v[b])) ++b;
            return v.substr(a, b - a);
        };
        auto fields = [&](const S& t) {
            int f = 0;
            for (size_t k = 0; k < t.size(); ++k) {
                if (t[k] == '\t') f++;
                else if (k + 2 < t.size() && t[k] == ' ' && t[k + 2] == ' ' && (t[k + 1] == '/' || t[k + 1] == '|')) f++;
            }
            return f;
        };
        auto dateStart = [&](const S& t) {  // 2026-10-02 / 2026.10.02 / 12:30 으로 시작
            S v = visText(t); size_t a = 0;
            while (a < v.size() && isWS(v[a])) ++a;
            size_t d = a; while (d < v.size() && v[d] >= '0' && v[d] <= '9') ++d;
            if (d == a || d >= v.size()) return false;
            if ((d - a == 4) && (v[d] == '-' || v[d] == '.' || v[d] == '/')) return true;
            return d - a <= 2 && v[d] == ':' && d + 2 < v.size() && v[d + 1] >= '0' && v[d + 1] <= '9';
        };
        auto speaker = [&](const S& t) {  // 대본·대화 기록: "민수: 대사" "민수 : 대사"
            S v = visText(t); size_t a = 0;
            while (a < v.size() && isWS(v[a])) ++a;
            size_t k = a;
            while (k < v.size() && k - a <= 10 && (isHangulSyl(v[k]) || isAsciiAlpha(v[k]) || isDigit(v[k]))) ++k;
            if (k == a || k - a > 10) return false;
            if (k < v.size() && v[k] == ' ') ++k;
            return k + 1 < v.size() && (v[k] == ':' || v[k] == 0xFF1A) && (v[k + 1] == ' ' || !isDigit(v[k + 1]));
        };
        int rec = 0, same = 0;
        S prevTok;
        for (int i = 0; i < n; ++i) {
            if (fields(lines[i].t) >= 2 || dateStart(lines[i].t) || speaker(lines[i].t)) rec++;
            S tok = firstTok(lines[i].t);
            if (i > 0 && !tok.empty() && tok == prevTok && endKind(lines[i - 1].t) != 2) same++;
            prevTok = tok;
        }
        listLike = rec * 10 >= n * 6 || (same * 10 >= (n - 1) * 6 && same >= 3);
    }
    R.listLike = listLike;
    if (listLike) for (auto& L : lines) L.head = false;  // 목록의 "- 항목", "[항목]" 줄도 같은 항목으로

    // 2) 빈 줄 구조
    int c0 = 0, c1 = 0;
    for (int i = 1; i < n; ++i) { if (lines[i].gap == 0) c0++; else if (lines[i].gap == 1) c1++; }
    int tot = n - 1;
    bool dbl = false;
    if (opt.blankMode == 2) dbl = true;
    else if (opt.blankMode == 0) dbl = tot >= 6 && c1 >= tot * 0.6 && c0 * 10 <= tot;  // 거의 모든 줄 사이가 빈 줄 (빈 줄 없는 줄바꿈이 섞여 있으면 아님)
    std::vector<char> brk(n, 1);  // i번 줄 앞이 문단 경계인가
    for (int i = 1; i < n; ++i) brk[i] = dbl ? (lines[i].gap >= 2) : (lines[i].gap >= 1);
    // 글 일부만 "줄마다 빈 줄"로 끊긴 경우: 글 전체로는 판정되지 않으므로 구간별로 찾음.
    // 빈 줄을 사이에 둔 줄들이 이어지는 구간에서, 둘레(앞뒤 12줄)의 줄 끝이 문장 중간에서 끊긴 것이 많으면
    // 그 구간의 빈 줄 1개는 문단 구분이 아니라 끊긴 줄 사이로 봄 (끝난 문장 뒤의 빈 줄은 그대로 문단 구분)
    int zoneGaps = 0;
    if (!dbl && opt.blankMode == 0 && opt.joinMode == 0 && !listLike) {
        const int K = 12;
        int s = 0;
        while (s < n) {
            int e = s;
            while (e + 1 < n && lines[e + 1].gap >= 1) ++e;
            if (e - s + 1 >= 6) {
                // k줄 뒤가 빈 줄 1개인 후보와 그중 끝나지 않은 줄의 누적 수
                std::vector<int> pc(e - s + 2, 0), pu(e - s + 2, 0);
                for (int k = s; k <= e; ++k) {
                    bool c = k < e && lines[k + 1].gap == 1 && !lines[k].head && !lines[k].sym;
                    pc[k - s + 1] = pc[k - s] + (c ? 1 : 0);
                    pu[k - s + 1] = pu[k - s] + (c && endKind(lines[k].t) == 0 ? 1 : 0);
                }
                for (int k = s; k < e; ++k) {
                    if (lines[k + 1].gap != 1) continue;
                    int a = std::max(s, k - K) - s, b = std::min(e, k + K + 1) - s;
                    int c = pc[b] - pc[a], u = pu[b] - pu[a];
                    if (c >= 4 && u >= 3 && u * 10 >= c * 3) lines[k].dz = true;
                }
            }
            s = e + 1;
        }
    }
    // 빈 줄 없이 이어진 줄 묶음의 크기
    for (int i = 0; i < n;) {
        int j = i + 1;
        while (j < n && lines[j].gap == 0) ++j;
        for (int k = i; k < j; ++k) lines[k].run = j - i;
        i = j;
    }
    // 같은 짧은 줄이 두 번 잇달아 나오면 제목 ("내 몸과의 생소한 만남" / "내 몸과의 생소한 만남")
    // 끊긴 구간의 첫 줄이 짧고 문장이 끝나지 않았으며 조사·쉼표로 끝나지도 않으면 제목
    if (!listLike) {
        static const S contEnd = U"게서로를을이가은는의와과고며면도만께에한인된던할및든나랑";
        for (int i = 0; i + 1 < n; ++i) {
            const Line &a = lines[i], &b = lines[i + 1];
            if (a.sym || b.sym || a.w > 40 || endKind(a.t) != 0 || isOpenQuote(firstVis(a.t))) continue;
            if (visText(a.t) == visText(b.t)) { lines[i].head = lines[i + 1].head = true; continue; }
            bool zoneStart = a.dz && (i == 0 || !(lines[i - 1].dz && a.gap == 1));
            C x = lastVis(a.t);
            if (zoneStart && !a.head && x != ',' && x != 0xFF0C && !(isHangulSyl(x) && contEnd.find(x) != S::npos) &&
                (i == 0 || endKind(lines[i - 1].t) != 0 || lines[i - 1].head || lines[i - 1].sym))
                lines[i].head = true;
        }
    }
    // 끊긴 줄 구간 확정: 끝나지 않은 줄들이 이어지다 문장부호로 끝나는 줄(~다. ~가? 등)에서 마무리되어야 끊긴 글.
    // 끝내 문장이 끝나지 않는 짧은 줄들(빈 줄 사이에 쓴 시·노래 가사 등)은 그대로 둠
    for (int i = 0; i < n; ++i) {
        if (!lines[i].dz) continue;
        auto open = [&](int k) { return lines[k].dz && !lines[k].head && !lines[k].sym && endKind(lines[k].t) == 0 && k + 1 < n && lines[k + 1].gap == 1; };
        if (!open(i)) { lines[i].dz = false; continue; }
        int j = i;
        while (j < n && open(j)) ++j;  // j: 사슬의 마지막 줄 (여기서 문장이 끝나야 함)
        bool ok = j < n && j - i <= 10 && !lines[j].head && !lines[j].sym && endKind(lines[j].t) == 2;
        for (int k = i; k < j; ++k) lines[k].dz = ok;
        if (j < n) lines[j].dz = false;
        i = j;
    }
    for (int i = 1; i < n; ++i) if (lines[i - 1].dz && lines[i].gap == 1) { brk[i] = 0; zoneGaps++; }
    // 빈 줄로 문단을 나누고, 문단 안에서는 줄만 바꾼 글: 문단 안의 줄바꿈은 결과에서도 줄바꿈만 (빈 줄 없이)
    // (끊긴 줄 사이로 본 빈 줄은 문단 구분으로 세지 않음)
    int blankSeps = 0;
    for (int i = 1; i < n; ++i) if (lines[i].gap >= 1 && brk[i]) blankSeps++;
    bool blockStyle = !dbl && blankSeps >= 2 && blankSeps >= (tot - zoneGaps) * 0.15 && c0 >= 2;

    // 3) 고정폭 줄바꿈 감지
    std::vector<int> ws;
    for (int i = 0; i + 1 < n; ++i) if (!brk[i + 1]) ws.push_back(lines[i].rw);
    int N = (int)ws.size();
    bool cut = false, ragged = false;
    int W = 0;
    if (N >= 8) {
        std::map<int, int> hist;
        for (int w : ws) if (w >= 16) hist[w]++;
        int best = 0, bestM = 0;
        for (auto& kv : hist) {
            int s = kv.second + (hist.count(kv.first - 1) ? hist[kv.first - 1] : 0);
            if (s > best || (s == best && kv.first > bestM)) { best = s; bestM = kv.first; }
        }
        if (bestM >= 16 && best >= N * 0.35) { cut = true; W = bestM; }
        else {
            std::vector<int> sw = ws; std::sort(sw.begin(), sw.end());
            int W95 = sw[(size_t)((sw.size() - 1) * 0.95)];
            if (W95 >= 30 && W95 <= 260) {
                int tol = std::max(8, W95 / 5), cnt = 0;
                for (int w : ws) if (w >= W95 - tol) cnt++;
                if (cnt >= N * 0.55) { ragged = true; W = W95; }
            }
        }
    }
    // 폭이 같은 줄이 많아도 그 줄들이 대부분 문장 끝(~다.)에서 끝나면 잘린 글이 아니라 이미 정리된 글
    if (cut || ragged) {
        int full = 0, ended = 0;
        for (int i = 0; i + 1 < n; ++i) {
            if (brk[i + 1] || lines[i].rw < W - 1) continue;
            full++;
            if (endKind(lines[i].t) == 2) ended++;
        }
        if (full >= 4 && ended * 10 >= full * 6) { cut = ragged = false; W = 0; }
    }
    if (listLike) { cut = ragged = false; W = 0; }
    bool wrap = cut || ragged;
    // a 줄이 "꽉 찬" 줄인가: 다음 줄 첫 글자/첫 단어가 a 뒤에 들어갈 자리가 없었으면 강제로 넘어간 것
    auto isFull = [&](const Line& a, const Line* b) {
        if (cut) {
            if (a.rw >= W - 1) return true;
            if (b && !b->t.empty()) return a.rw + cw(b->t[0]) + (b->lead > 0 ? 1 : 0) > W;
            return false;
        }
        if (ragged) {
            if (a.rw >= W - 1) return true;
            if (b) {
                int fw = 0;
                for (C c : b->t) { if (c == ' ') break; fw += cw(c); }
                return a.rw + 1 + fw > W;
            }
            return false;
        }
        return false;
    };

    // 고정폭으로 꽉 찬 줄은 제목이 아니라 잘린 본문
    if (wrap) for (int i = 0; i + 1 < n; ++i) if (lines[i].head && !brk[i + 1] && lines[i].rw >= W - 2) lines[i].head = false;
    // 첫 줄이 짧고 끝나지 않은 글이면 제목으로 봄 (단, 고정폭으로 꽉 찬 줄은 본문이 잘린 것)
    if (n > 1 && !lines[0].sym && !lines[0].head && lines[0].w <= 40 && endKind(lines[0].t) == 0 &&
        !isOpenQuote(firstVis(lines[0].t)) && lastVis(lines[0].t) != ',' && lastVis(lines[0].t) != 0xFF0C &&  // 대사 첫 줄·쉼표로 끝난 줄은 제목 아님
        !(wrap && isFull(lines[0], &lines[1])) && (lines[0].w <= 30 || lines[1].gap >= 1))
        lines[0].head = true;

    // 4) 들여쓰기 = 문단 시작?
    int indentMin = cut ? 2 : 1, ci = 0;
    for (int i = 1; i < n; ++i) if (lines[i].lead >= indentMin) ci++;
    bool indentMarks = ci >= 3 && ci >= tot * 0.03 && ci <= tot * 0.7;
    if (cut && indentMarks) {
        // 고정폭 글에서 들여쓴 줄 바로 앞 줄이 꽉 차 있으면 들여쓰기가 아니라 잘림 공백
        int shortBefore = 0, cnt = 0;
        for (int i = 1; i < n; ++i) if (lines[i].lead >= indentMin && !brk[i]) { cnt++; if (!isFull(lines[i - 1], &lines[i])) shortBefore++; }
        if (cnt > 0 && shortBefore < cnt * 0.4) indentMarks = false;
    }

    // 5) 끊긴 줄이 많은가
    int cand = 0, unterm = 0;
    for (int i = 0; i + 1 < n; ++i) {
        if (brk[i + 1] || lines[i].head || lines[i].sym) continue;
        cand++;
        if (endKind(lines[i].t) == 0) unterm++;
    }
    bool joinAllowed = opt.joinMode == 0 && (cand < 10 || unterm >= cand * 0.15);
    // 글 전체로는 끊긴 줄이 적어도, 끊긴 줄이 몰려 있는 구간(앞뒤 15줄 안팎)은 이음
    if (opt.joinMode == 0 && !listLike) {
        std::vector<int> ci2;
        for (int i = 0; i + 1 < n; ++i) if (!brk[i + 1] && !lines[i].head && !lines[i].sym) ci2.push_back(i);
        int m = (int)ci2.size();
        std::vector<int> pu(m + 1, 0);
        for (int k = 0; k < m; ++k) pu[k + 1] = pu[k] + (endKind(lines[ci2[k]].t) == 0 ? 1 : 0);
        const int K = 15;
        for (int k = 0; k < m; ++k) {
            int a = std::max(0, k - K), b = std::min(m, k + K + 1);
            int c = b - a, u = pu[b] - pu[a];
            if (u >= 3 && u * 10 >= c * 3) lines[ci2[k]].lj = true;
        }
    }

    // 머리글·출처처럼 문장이 아닌 줄: "제목 - 작가" 줄, 주소(URL)로 끝나는 줄
    auto notSentence = [&](const Line& L) {
        if (L.w > 60) return false;
        S v = visText(L.t);
        if (endKind(v) == 2) return false;
        if (v.find(U" - ") != S::npos || v.find(U" – ") != S::npos || v.find(U" — ") != S::npos) return true;
        size_t sp = v.find_last_of(U' ');
        S last = sp == S::npos ? v : v.substr(sp + 1);
        return startsWithCI(last, "http") || startsWithCI(last, "www.");
    };
    auto shouldJoin = [&](const Line& a, const Line& b) {
        if (opt.joinMode == 1 || listLike) return false;
        {   // 줄 전체가 문장부호 하나뿐: "뻗었다\n." / "안녕\n?"
            bool allP = !b.t.empty() && b.t.size() <= 3;
            for (C c : b.t) if (!(c == '.' || c == '!' || c == '?' || c == 0x201D || c == 0x2019 || c == '"' || c == 0x300D || c == 0x300F)) allP = false;
            if (allP && !a.sym && !(b.t.size() >= 2 && b.t[0] == '.' && b.t[1] == '.')) return true;
        }
        if (a.sym && b.sym) return true;  // "* *" / "*" 처럼 쪼개진 구분선
        if (a.sym || b.sym || a.head || b.head) return false;
        if (!wrap && (notSentence(a) || notSentence(b))) return false;
        // 가운데 정렬로 쓴 머리말·경고문 등: 둘 다 크게 들여쓴 줄이고 앞 줄이 조사·연결 어미로 끝나면
        // 한 문장이 줄바꿈된 것 ("수험생에게 / 판매, 대여…", "인한 / 결석이…")
        // 그 밖의 글에서도 조사·연결 어미로 끝난 줄은 문장 중간에서 바뀐 것으로 보고 이음
        // (한 줄 = 한 문단인 글, 들여쓰기로 문단을 나눈 글에서도). 짧은 줄(제목 등)은 제외
        if (endKind(a.t) == 0) {
            static const S cont = U"게서로를을이가은는의와과고며면도만께에한인된던할및든나랑";
            C x = lastVis(a.t);
            size_t vis = 0; int words = 1;
            for (size_t k = 0; k < a.t.size(); ++k) {
                C c = a.t[k];
                if (!isInvisible(c) && !isWS(c)) vis++;
                if (isWS(c) && k + 1 < a.t.size() && !isWS(a.t[k + 1])) words++;
            }
            bool centered = a.lead >= 6 && b.lead >= 6;
            S bv = visText(b.t); size_t bl = 0;
            bool bTitle = !bv.empty() && chapterAt(bv, 0, bl);
            if (isHangulSyl(x) && cont.find(x) != S::npos && isLetterDigit(firstVis(b.t)) && !bTitle &&
                (centered ? vis >= 8 : (vis >= 12 && words >= 3)))
                return true;
        }
        if (indentMarks && b.lead >= indentMin) return false;
        C x = lastVis(a.t), y = firstVis(b.t);
        if (x == ',' || x == 0xFF0C || x == 0x3001) return true;
        // 줄 첫머리가 문장부호: "했다\n." / "그래\n?" / "안녕\n”"
        bool yPunct = y == ',' || y == '!' || y == '?' || y == ')' || y == ']' || y == 0x201D || y == 0x2019 ||
                      y == 0x300D || y == 0x300F || y == 0xFF0C || y == 0x3001 || (y == '.' && (b.t.size() < 2 || b.t[1] != '.'));
        if (yPunct && !b.sym) return true;
        // 여는 따옴표/괄호로 끝난 줄: "“\n안녕"
        if (x == 0x201C || x == 0x2018 || x == 0x300C || x == 0x300E || x == '(' || x == '[') return true;
        // 글 일부만 "줄마다 빈 줄"로 끊긴 구간: 문장이 끝나지 않은 줄은 이음
        if (a.dz && b.gap == 1) return endKind(a.t) == 0;
        if (wrap) return isFull(a, &b);
        // 빈 줄로 문단을 나눈 글에서 문단 안의 줄바꿈은 작가가 일부러 한 것 (시·노래 가사·편지 등):
        // 조사·연결 어미로 끝난 줄(위에서 처리)만 잇고 나머지는 그대로 둠
        if (blockStyle) {
            // 한 글자만 다음 줄로 넘어간 것("뻗었 / 다.")은 이음
            S bv = visText(b.t); size_t k = 0;
            while (k < bv.size() && isHangulSyl(bv[k])) ++k;
            bool rest = true;
            for (size_t q = k; q < bv.size(); ++q) if (!isTermChar(bv[q]) && !isCloseQuote(bv[q])) rest = false;
            if ((joinAllowed || a.lj) && k == 1 && rest && isHangulSyl(lastVis(a.t)) && endKind(a.t) == 0) return true;
            // 긴 줄(또는 줄 안에서 문장이 끝난 줄)이 문장 중간에서 끊긴 것은 이음 — 짧은 줄만 이어진 시·편지는 그대로
            // 빈 줄 없이 길게(8줄 이상) 이어진 묶음에 끊긴 줄이 몰려 있으면 시·편지가 아니라 끊긴 글
            if (a.lj && a.run >= 8 && b.gap == 0) return endKind(a.t) == 0;
            bool innerEnd = false;
            for (size_t q = 0; q + 1 < a.t.size(); ++q) if (isTermChar(a.t[q]) && isWS(a.t[q + 1])) { innerEnd = true; break; }
            return (joinAllowed || a.lj) && endKind(a.t) == 0 && (a.w >= 30 || innerEnd);
        }
        return (joinAllowed || a.lj) && endKind(a.t) == 0;
    };
    auto joinSep = [&](const Line& a, const Line& b) -> S {
        if (opt.spaceMode == 1) return U" ";
        if (opt.spaceMode == 2) return U"";
        C x = lastVis(a.t), y = firstVis(b.t);
        if (x == ',' || x == 0xFF0C || x == 0x3001) return U" ";
        if (y == ',' || y == '.' || y == '!' || y == '?' || y == ')' || y == ']' || y == 0x201D || y == 0x2019 ||
            y == 0x300D || y == 0x300F || y == 0xFF0C || y == 0x3001) return U"";
        if (x == 0x201C || x == 0x2018 || x == 0x300C || x == 0x300E || x == '(' || x == '[') return U"";
        if (isHangulSyl(x) && isHangulSyl(y) && endKind(a.t) == 0) {
            // 다음 줄이 한 글자(+문장부호)뿐이면 단어가 쪼개진 것: "뻗었\n다."
            size_t k = 0; while (k < b.t.size() && isHangulSyl(b.t[k])) ++k;
            bool rest = true; for (size_t q = k; q < b.t.size(); ++q) if (!isTermChar(b.t[q]) && !isCloseQuote(b.t[q])) rest = false;
            if (k == 1 && rest) return U"";
        }
        if (x == '-' && a.t.size() >= 2 && isAsciiAlpha(a.t[a.t.size() - 2]) && isAsciiAlpha(y)) return U"";
        if (isKanaHan(x) && isKanaHan(y)) return U"";
        if (a.sym && b.sym) return U" ";
        if (a.trail || b.lead > 0) return U" ";
        if (cut) return U"";
        return U" ";
    };

    // 6) 문단 만들기
    struct Para { TM x; int kind; int gap = 0; bool scene = false; bool noScene = false; bool cont = false; bool soft = false; int startQ = 0; bool fromLong = false; bool inQuote = false; bool speechCont = false; bool speechEnd = false; };  // speechCont/End: 긴 대사를 나눈 둘째 줄부터 / 마지막 줄  // inQuote: 문단 전체가 대사 속(닫는 따옴표는 뒤 문단에)  // fromLong: 원래 400자가 넘던 문단(한 줄 글 등)에서 나온 조각  // soft: 빈 줄로 문단을 나눈 글의 문단 안 줄바꿈  // cont: 앞 조각과 같은 원문 문단(문장마다 나눈 것)  // kind: 0 본문, 1 제목, 2 구분선
    auto lineTM = [&](const Line& L) { TM r; r.t = L.t; r.m = L.m; return r; };
    std::vector<Para> paras;
    {
        Para cur{lineTM(lines[0]), lines[0].sym ? 2 : lines[0].head ? 1 : 0, 0};
        int parts = 1;
        for (int i = 1; i < n; ++i) {
            if (!brk[i] && shouldJoin(lines[i - 1], lines[i])) {
                cur.x.add(joinSep(lines[i - 1], lines[i]), lines[i].m[0]);
                cur.x.add(lineTM(lines[i]));
                parts++;
                R.joined = true;
            } else {
                if (parts > 1) cur.kind = 0;
                paras.push_back(std::move(cur));
                cur = Para{lineTM(lines[i]), lines[i].sym ? 2 : lines[i].head ? 1 : 0, lines[i].gap};
                cur.soft = (blockStyle || listLike) && lines[i].gap == 0;
                cur.noScene = lines[i].afterJunk;
                parts = 1;
            }
        }
        if (parts > 1) cur.kind = 0;
        paras.push_back(std::move(cur));
    }
    std::vector<Line>().swap(lines);  // 큰 파일 메모리 절약: 줄 정보는 더 쓰지 않음

    // 7) 문장/대사 나누기
    // 장면 전환 판정: 문단 사이 간격(빈 줄 수) 중 가장 흔한 값보다 넓은 곳이 "드물게" 있으면 의도된 장면 전환
    if (paras.size() >= 4) {
        int hist[4] = {0, 0, 0, 0};
        int total = 0;
        for (size_t i = 1; i < paras.size(); ++i) if (!paras[i].noScene && !paras[i].soft) { hist[std::min(paras[i].gap, 3)]++; total++; }
        int base = 0;
        for (int g = 1; g < 4; ++g) if (hist[g] > hist[base]) base = g;
        int wider = 0;
        for (size_t i = 1; i < paras.size(); ++i) if (!paras[i].noScene && !paras[i].soft && std::min(paras[i].gap, 3) > base) wider++;
        if (wider > 0) {
            // 의도된 장면 전환: 넓은 간격이 드물고(1/3 이하), 넓은 간격의 크기가 한 가지로 일정하며,
            // 평소보다 좁은 간격도 거의 없을 때. 간격이 여러 크기로 뒤섞이면 제각각인 글로 보고 통일함
            int widerKinds = 0, narrower = 0;
            for (int g = base + 1; g < 4; ++g) if (hist[g]) widerKinds++;
            for (int g = 0; g < base; ++g) narrower += hist[g];
            bool intentional = wider * 3 <= total && widerKinds == 1 && narrower * 10 <= total;
            if (intentional) {
                for (size_t i = 1; i < paras.size(); ++i) {
                    if (paras[i].noScene || paras[i].soft || std::min(paras[i].gap, 3) <= base) continue;
                    // 이미 구분선(* * *)이나 제목이 있는 곳은 따로 표시하지 않음
                    if (paras[i].kind != 0 || paras[i - 1].kind != 0) continue;
                    paras[i].scene = true;
                    R.sceneBreaks++;
                }
            } else {
                R.unevenGaps = true;
            }
        }
        // 간격 종류가 여러 가지인데 장면 전환이 아니면 "제각각"
        int kinds = 0;
        for (int g = 0; g < 4; ++g) if (hist[g]) kinds++;
        if (kinds >= 2 && R.sceneBreaks == 0 && hist[base] < total) R.unevenGaps = true;
    }

    // 두 칸 공백 문단 나누기 (긴 문단만)
    {
        std::vector<Para> tmp;
        for (auto& p : paras) {
            if (p.kind == 0 && !listLike && (int)p.x.t.size() > 30) {
                auto parts = splitWideGaps(p.x, R.wideGapBreaks);
                bool first = true;
                for (auto& q : parts) {
                    if (q.t.empty()) continue;
                    std::vector<TM> cs;
                    if ((int)q.t.size() > 30) cs = splitChapters(q, R.chapterBreaks); else cs.push_back(q);
                    for (auto& c : cs) {
                        if (c.t.empty()) continue;
                        Para np{c, 0, first ? p.gap : 0, first && p.scene};
                        np.cont = first && p.cont;
                        np.soft = first && p.soft;
                        np.startQ = first ? p.startQ : 0;
                        first = false;
                        TM tt, rr;
                        if (splitTitleHead(np.x, tt, rr)) {  // 제목과 본문을 나눔
                            Para tp = np; tp.x = tt; tp.kind = 1;
                            tmp.push_back(tp);
                            Para bp{rr, 0, 0};
                            tmp.push_back(bp);
                            continue;
                        }
                        tmp.push_back(np);
                    }
                }
            } else {
                TM tt, rr;
                if (p.kind == 0 && splitTitleHead(p.x, tt, rr)) {  // 짧은 문단도 "제2화 만남 아침이 밝았다." 이면 제목과 본문을 나눔
                    Para tp = p; tp.x = tt; tp.kind = 1;
                    Para bp{rr, 0, 0};
                    tmp.push_back(tp);
                    tmp.push_back(bp);
                } else tmp.push_back(std::move(p));
            }
        }
        paras.swap(tmp);
    }
    // 이 글이 대사를 따로 줄에 두는 스타일인가: 대사로 시작하는 문단 수 vs 서술 중간에 대사가 박힌 문단 수
    // (근거가 적으면 한국 웹소설 관례대로 따로 줄로 봄)
    auto hasInlineDialogue = [](const S& t) {
        int dq = 0;  // 곧은 따옴표는 짝수 번째 뒤가 여는 따옴표
        for (size_t i = 0; i < t.size(); ++i) {
            C c = t[i];
            bool opening = c == 0x201C || c == 0x300C || c == 0x300E;
            if (c == '"') { opening = dq % 2 == 0; dq++; }
            if (!opening || i < 2) continue;
            if (!isWS(t[i - 1])) continue;
            size_t k = i - 1;
            while (k > 0 && isWS(t[k - 1])) --k;
            if (k > 0 && (isTermChar(t[k - 1]) || isCloseQuote(t[k - 1]))) return true;
        }
        return false;
    };
    // 여러 줄에 걸친 대사: 앞 문단 끝에서 열린 따옴표가 이 문단에서 닫히면 이어진 대사로 봄
    {
        // 따옴표 수 세기: 곧은 따옴표 수, 여는 둥근 따옴표 수, 닫는 둥근 따옴표 수
        auto qcount = [](const S& v, int& dq, int& qo, int& qc) {
            dq = qo = qc = 0;
            for (C c : v) { if (c == '"') dq++; else if (qCloseOf(c)) qo++; else if (isQClose(c)) qc++; }
        };
        std::vector<S> vis(paras.size());
        for (size_t i = 0; i < paras.size(); ++i) if (paras[i].kind == 0) vis[i] = visText(paras[i].x.t);
        std::vector<char> dead(paras.size(), 0);
        int carry = 0;
        size_t openIdx = 0;  // 지금 열린 대사가 시작된 문단
        for (size_t i = 0; i < paras.size(); ++i) {
            auto& p = paras[i];
            if (p.kind != 0) { carry = 0; continue; }
            if (carry) {
                // 뒤에서 대사가 닫히는 문단 찾기 (그 사이 문단에는 따옴표가 없어야 함)
                size_t k = i, chars = 0;
                bool found = false;
                for (; k < paras.size() && k < i + 20 && chars < 3000; ++k) {
                    if (paras[k].kind != 0) break;
                    int d2, o2, c2;
                    qcount(vis[k], d2, o2, c2);
                    if (carry == 1 ? (d2 % 2 == 1) : (c2 > o2)) { found = true; break; }
                    if (d2 || o2 || c2) break;
                    chars += vis[k].size();
                }
                if (found) {
                    // 여러 줄에 걸친 대사는 한 문단으로 합침: 짧으면 한 줄, 길면 나중에 대사 안에서 줄만 바꿈
                    Para& op = paras[openIdx];
                    for (size_t q = i; q <= k; ++q) {
                        op.x.add(U' ', paras[q].x.first());
                        op.x.add(paras[q].x);
                        dead[q] = 1;
                    }
                    vis[openIdx] = visText(op.x.t);
                    std::vector<char> role; int endQ = 0;
                    encloseRoles(vis[openIdx], op.startQ, role, endQ);
                    carry = endQ;
                    i = k;
                    continue;
                }
            }
            std::vector<char> role; int endQ = 0;
            encloseRoles(vis[i], p.startQ, role, endQ);
            carry = endQ;
            openIdx = i;
        }
        std::vector<Para> kept;
        kept.reserve(paras.size());
        for (size_t i = 0; i < paras.size(); ++i) if (!dead[i]) kept.push_back(std::move(paras[i]));
        paras.swap(kept);
    }
    bool autoDialogue;
    {
        int standalone = 0, inlineCnt = 0;
        for (auto& p : paras) {
            if (p.kind != 0) continue;
            if (isDialogue(p.x.t)) standalone++;
            else if (hasInlineDialogue(visText(p.x.t))) inlineCnt++;
        }
        autoDialogue = standalone >= inlineCnt || standalone + inlineCnt < 5;
    }
    // 짧고 문단 구분이 거의 없는 글(줄거리·작품 소개 등): 대사가 박힌 문단은 문장마다 나눔
    bool shortDoc;
    {
        size_t chars = 0; int cnt = 0;
        for (auto& p : paras) if (p.kind == 0) { cnt++; chars += visText(p.x.t).size(); }
        shortDoc = cnt <= 3 && chars <= 800;
    }
    // 첫 회차 제목 앞의 머리글(줄거리·작품 소개): 본문과 달리 문장마다 나눔
    size_t frontEnd = 0;
    {
        size_t firstTitle = paras.size();
        for (size_t i = 0; i < paras.size(); ++i) {
            if (paras[i].kind != 1) continue;
            S vt = visText(paras[i].x.t);
            while (!vt.empty() && isWS(vt[0])) vt.erase(0, 1);
            size_t len;
            if (!vt.empty() && chapterAt(vt, 0, len)) { firstTitle = i; break; }  // 회차 제목만 (쪽 번호·구분 줄 제외)
        }
        int frontCnt = 0;
        for (size_t i = 0; i < firstTitle; ++i) if (paras[i].kind == 0) frontCnt++;
        if (firstTitle < paras.size() && frontCnt >= 1 && frontCnt <= 5) frontEnd = firstTitle;
    }
    size_t paraIdx = 0;

    std::vector<CompiledRegex> rx = compileRegex(opt.regexReplaces);
    std::vector<Para> outP;
    for (auto& p : paras) {
        bool front = paraIdx++ < frontEnd;
        size_t firstOut = outP.size();
        struct SceneFix { std::vector<Para>& v; size_t at; bool scene; ~SceneFix() { if (scene && at < v.size()) v[at].scene = true; } } sf{outP, firstOut, p.scene};
        struct Release { TM& x; ~Release() { S().swap(x.t); std::vector<int>().swap(x.m); } } rel{p.x};  // 다 쓴 문단은 바로 비움
        if (opt.collapseSp)
            p.x = (p.kind == 0) ? visApply(p.x, [&](const TM& v) { return fixSpacing(v, R.spaceFixes, R.letterSpaced, p.startQ); })
                                : visApply(p.x, [](const TM& v) { return collapseSpaces(v); });
        if (opt.quoteMode || opt.ellipsisMode || opt.stripHanja || opt.fullToHalf) p.x = charStyle(p.x, opt, R.charFixes);
        if (!opt.replaces.empty()) p.x = visApply(p.x, [&](const TM& v) { return applyReplaces(v, opt, R.replaceHits); });
        if (!rx.empty()) p.x = visApply(p.x, [&](const TM& v) { return applyRegexTM(v, rx, R.replaceHits); });
        if (p.x.t.empty()) continue;
        if (p.kind != 0 || opt.splitMode == 2 || listLike) { outP.push_back(std::move(p)); continue; }
        S pv = visText(p.x.t);
        bool isLong = (int)pv.size() > opt.longLen;
        bool embedded = autoDialogue && hasInlineDialogue(pv);  // 서술 중간에 박힌 대사
        if (opt.splitMode == 0 && !isLong && !embedded && !front) { outP.push_back(std::move(p)); continue; }
        std::vector<TM> units = splitUnits(p.x, p.startQ);
        bool firstPara = paraIdx == 1 && (!isLong || paras.size() > 1);  // 글 맨 앞의 문단(제목보다도 앞): 줄거리·소개글 자리
        bool sentencePath = opt.splitMode == 1 || (embedded && (shortDoc || firstPara)) || front;
        if (units.size() <= 1 && !(sentencePath && visText(p.x.t).size() >= kLongSpeech)) { outP.push_back(std::move(p)); continue; }
        // 짧은 글에서 서술 속에 대사가 박힌 문단(줄바꿈이 사라진 줄거리·소개글)은 문장마다 나눔.
        // 말줄임표(…… ...)는 문장 끝으로 보지 않으므로 "성해비치…… 수년 후……" 는 이어짐
        if (sentencePath) {
            // 말줄임표로 끝난 조각("성해비치…… 수년 후……")은 다음 문장과 이어 두고,
            // 대사 바로 뒤의 짧은 말꼬리("수진이 물었다.")는 대사에 붙임
            TM cur; bool lastDlg = false; bool pushed = p.cont; bool firstSoft = p.soft;
            bool srcLong = pv.size() > 400;  // 작가가 나눈 보통 문단이 아니라 뭉친 글
            auto endsEllipsis = [](const S& t) {
                size_t k = t.size();
                while (k > 0 && (isWS(t[k - 1]) || isInvisible(t[k - 1]))) --k;
                return k > 0 && (t[k - 1] == 0x2026 || t[k - 1] == 0x2025 || (t[k - 1] == '.' && k >= 2 && t[k - 2] == '.'));
            };
            for (size_t ui = 0; ui < units.size(); ++ui) {
                auto& u = units[ui];
                std::vector<TM> sp = splitLongSpeech(u, ui == 0 ? p.startQ : 0);
                if (sp.size() > 1) {  // 긴 대사: 대사 안에서 줄만 바꿈
                    if (!cur.t.empty()) { Para q{cur, 0, 0}; q.cont = pushed; q.soft = firstSoft; q.fromLong = srcLong; firstSoft = false; outP.push_back(q); pushed = true; cur = TM(); }
                    for (size_t k = 0; k < sp.size(); ++k) {
                        Para q{sp[k], 0, 0};
                        q.cont = k ? true : pushed; q.soft = k == 0 && firstSoft; q.fromLong = srcLong;
                        q.speechCont = k > 0; q.speechEnd = k + 1 == sp.size();
                        outP.push_back(q);
                    }
                    firstSoft = false; pushed = true; lastDlg = false;
                    continue;
                }
                bool dlg = isDialogue(u.t);
                bool sym = isSymbolLine(u.t);
                bool glue = !cur.t.empty() && !dlg && !sym &&
                            (!lastDlg && endsEllipsis(cur.t));
                if (!glue && !cur.t.empty()) { Para q{cur, 0, 0}; q.cont = pushed; q.soft = firstSoft; q.fromLong = srcLong; firstSoft = false; outP.push_back(q); pushed = true; cur = TM(); }
                if (sym) { outP.push_back({u, 2, 0}); pushed = false; lastDlg = false; continue; }
                if (!cur.t.empty()) cur.add(U' ', u.first());
                cur.add(u);
                lastDlg = dlg;
            }
            if (!cur.t.empty()) { Para q{cur, 0, 0}; q.cont = pushed; q.soft = firstSoft; q.fromLong = srcLong; outP.push_back(q); }
            continue;
        }
        // 나누는 기준(longLen)보다 조각은 작게(약 60%): 작가가 쓴 조금 긴 문단은 그대로 두면서,
        // 한 줄로 뭉친 글은 읽기 좋은 길이로 나뉘게 함
        size_t limit = isLong ? (size_t)std::max(40, opt.longLen * 6 / 10) : 0;
        TM chunk;
        bool firstChunk = true;
        auto flushChunk = [&]() {
            if (chunk.t.empty()) return;
            Para q{chunk, 0, 0}; q.cont = firstChunk && p.cont; q.soft = firstChunk && p.soft; firstChunk = false;
            outP.push_back(q); chunk = TM();
        };
        for (auto& u : units) {
            if (isSymbolLine(u.t)) { flushChunk(); outP.push_back({u, 2, 0}); continue; }
            // 대사는 새 문단에서 시작 (대사 뒤에 붙는 "그가 말했다." 같은 서술은 그대로 이어 붙임)
            if (autoDialogue && isDialogue(u.t) && !chunk.t.empty()) flushChunk();
            else if (!chunk.t.empty() && limit && visText(chunk.t).size() + 1 + visText(u.t).size() > limit) flushChunk();
            if (!chunk.t.empty()) chunk.add(U' ', u.first());
            chunk.add(u);
        }
        flushChunk();
    }

    // 8) 출력
    TM out;
    out.t.reserve(in.size() + in.size() / 8);
    out.m.reserve(in.size() + in.size() / 8);
    int prevKind = -1;
    bool prevDlg = false, prevSpecial = false;
    auto special = [](const Para& q) {  // 짧은 줄만 검사 (긴 문단은 해당 없음)
        return q.kind == 0 && q.x.t.size() < 300 && (isBracketLine(q.x.t) || isSoundLine(q.x.t));
    };
    // 꾸밈 제목: 원문에서 빈 줄 없이 붙어 있던 "구분선 / 제목 / 구분선"은 한 덩어리로 (사이에 빈 줄을 넣지 않음)
    auto titleish = [&](size_t i) {
        const Para& q = outP[i];
        if (q.kind == 1) return true;
        return q.kind == 0 && !q.cont && !q.fromLong && visText(q.x.t).size() <= 40 && q.x.t.find(U'\n') == S::npos;
    };
    auto tight = [&](size_t i) { return i < outP.size() && outP[i].gap == 0 && !outP[i].scene; };
    auto frameLink = [&](size_t i) {  // outP[i-1]과 outP[i] 사이
        if (i == 0 || !tight(i)) return false;
        const Para &a = outP[i - 1], &b = outP[i];
        if (a.kind == 2 && b.kind != 2 && titleish(i))  // 구분선 → 제목: 제목 뒤에도 구분선이 붙어 있거나, 제목으로 알아본 줄
            return b.kind == 1 || (i + 1 < outP.size() && outP[i + 1].kind == 2 && tight(i + 1));
        if (b.kind == 2 && a.kind != 2 && titleish(i - 1))  // 제목 → 구분선
            return a.kind == 1 || (i >= 2 && outP[i - 2].kind == 2 && tight(i - 1));
        return false;
    };
    for (size_t pi = 0; pi < outP.size(); ++pi) {
        auto& p = outP[pi];
        int f = p.x.first();
        if (prevKind >= 0 && frameLink(pi)) {
            out.add(U"\n", f);
        } else if (prevKind >= 0) {
            // 원문 한 문단을 문장마다 나눈 줄 사이는 줄바꿈만, 원문 문단 사이는 빈 줄
            // 대사 앞뒤는 빈 줄로 띄움
            bool dlgNear = isDialogue(p.x.t) || prevDlg;
            // 원문에서 이미 줄만 바꾼 곳(soft)은 그대로. 우리가 문장마다 나눈 곳(cont)은 줄만 바꾸되,
            // 대사·시스템 메시지·소리 한 마디 앞뒤와 장면·시간이 바뀌는 첫머리("한편," "다음 날")는 빈 줄로 띄움
            // 장면 전환·알림 줄·소리 줄 띄우기는 뭉친 글(400자 넘는 문단)을 나눌 때만: 작가가 나눈 보통 문단은 건드리지 않음
            bool extra = p.fromLong && (prevSpecial || special(p) || startsTransition(p.x.t));
            bool joinLine = p.soft || p.speechCont || (p.cont && !dlgNear && !extra);
            // 제목·구분선 앞뒤는 빈 줄로 띄우되, 원문에서 빈 줄 없이 붙여 쓴 곳(soft)은 원문대로 줄만 바꿈
            bool blank = (opt.paraSep == 0 && !joinLine) || ((p.kind != 0 || prevKind != 0) && !p.soft);
            if (p.scene && opt.sceneMode == 1) {
                out.add(U"\n\n* * *\n\n", f);  // 장면 전환 표시
            } else {
                out.add(blank ? U"\n\n" : U"\n", f);
                if (p.scene && opt.sceneMode == 0) out.add(U'\n', f);  // 빈 줄 하나 더
            }
        }
        if (p.kind == 0) {
            if (opt.indent == 1) out.add(U" ", f);
            else if (opt.indent == 2) out.add(U"  ", f);
            else if (opt.indent == 3) out.add(U"　", f);
        }
        out.add(p.x);
        prevKind = p.kind;
        prevDlg = p.kind == 0 && (isDialogue(p.x.t) || p.speechEnd);
        prevSpecial = pi + 1 < outP.size() && outP[pi + 1].cont && outP[pi + 1].fromLong && special(p);  // 다음 줄이 이어진 줄일 때만 필요
    }
    out.add(U'\n', (int)in.size());

    R.outParas = (int)outP.size();
    R.doubleSpaced = dbl && opt.blankMode == 0;
    R.wrapWidth = wrap ? W : 0;
    R.wrapCut = cut;
    R.indentMarks = indentMarks;
    if (rep) *rep = R;
    if (outMap) outMap->swap(out.m);
    return out.t;
}

// 둥근 따옴표와 곧은 따옴표가 섞여 짝을 이루는 경우("“대사" / ""대사”")를 찾아
// 곧은 쪽을 둥근 짝으로 보고 처리한 뒤, 결과에서는 원래 곧은 따옴표로 되돌린다.
static const size_t kMixQuoteSpan = 3000;
std::u32string TidyProcess(const std::u32string& in, const TidyOptions& opt, TidyReport* rep, std::vector<int>* outMap) {
    std::vector<size_t> conv;   // 둥근 따옴표로 바꿔 본 곧은 따옴표 위치
    bool anyCurly = false;
    for (C c : in) if (c == 0x201C || c == 0x201D) { anyCurly = true; break; }
    if (!anyCurly) return TidyCore(in, opt, rep, outMap);
    std::u32string w = in;
    int open = 0; size_t op = 0;  // 0 없음, 1 둥근 여는 따옴표, 2 곧은 여는 따옴표
    for (size_t i = 0; i < w.size(); ++i) {
        C c = w[i];
        if (c != '"' && c != 0x201C && c != 0x201D) continue;
        if (open && i - op > kMixQuoteSpan) open = 0;
        if (c == 0x201C) { open = 1; op = i; }
        else if (c == 0x201D) {
            if (open == 2) { w[op] = 0x201C; conv.push_back(op); }
            open = 0;
        } else {  // 곧은 따옴표
            if (open == 1) { w[i] = 0x201D; conv.push_back(i); open = 0; }
            else if (open == 2) open = 0;
            else { open = 2; op = i; }
        }
    }
    if (conv.empty()) return TidyCore(in, opt, rep, outMap);
    std::vector<int> m;
    std::u32string out = TidyCore(w, opt, rep, &m);
    if (opt.quoteMode == 0) {  // "그대로": 바꿔 본 따옴표를 원래 곧은 따옴표로 되돌림
        std::sort(conv.begin(), conv.end());
        for (size_t k = 0; k < out.size() && k < m.size(); ++k) {
            if (out[k] != 0x201C && out[k] != 0x201D) continue;
            if (m[k] < 0) continue;
            if (std::binary_search(conv.begin(), conv.end(), (size_t)m[k])) out[k] = '"';
        }
    }
    if (outMap) outMap->swap(m);
    return out;
}
