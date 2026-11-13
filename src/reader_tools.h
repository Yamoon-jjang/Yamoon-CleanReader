// 야문 클린리더 — 화면 기능을 돕는 계산 (플랫폼 독립, 헤더만)
//  - 바꾸기의 조사 맞춤: 이름을 바꿀 때 뒤에 붙은 조사를 새 이름의 받침에 맞춤 (서윤은 → 유나는)
//  - 목차의 회차 번호 확인: 빠진 번호·겹친 번호 찾기
//  - 정리 모드의 바뀐 곳 표시: 결과에서 바뀐 글자, 원본에서 지워진 글자 구간
// 정리 엔진(engine.cpp)과는 따로 둠: 여기를 고쳐도 정리 결과는 달라지지 않음.
// tests/check_reader_tools.py 가 이 파일을 그대로 컴파일해 시험함.
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstddef>

namespace rt {

// ---------------------------------------------------------------------------
// 조사 맞춤
inline bool Syl(wchar_t c) { return c >= 0xAC00 && c <= 0xD7A3; }
inline int Jong(wchar_t c) { return Syl(c) ? (int)((c - 0xAC00) % 28) : -1; }  // 0 받침 없음, 8 ㄹ
inline bool At(const std::wstring& t, size_t p, const wchar_t* w) {
    for (; *w; ++w, ++p) if (p >= t.size() || t[p] != *w) return false;
    return true;
}
inline size_t Len(const wchar_t* w) { size_t n = 0; while (w[n]) ++n; return n; }
inline bool In(wchar_t c, const wchar_t* set) { for (; *set; ++set) if (*set == c) return true; return false; }
// 조사 뒤에 이어져도 되는 글자 (서윤과의, 유나는요 같은 겹조사). 이 밖의 한글이 이어지면 조사가 아니라 다른 낱말로 봄
inline bool JosaTailOk(const std::wstring& t, size_t p) {
    if (p >= t.size() || !Syl(t[p])) return true;
    return In(t[p], L"의은는도만에요까");
}
struct JosaPair { const wchar_t* c; const wchar_t* v; };  // c: 받침 있는 말 뒤, v: 받침 없는 말 뒤
// 긴 것부터 (앞부분이 같은 짧은 조사보다 먼저 맞춰 봄)
inline const JosaPair* JosaPairs(size_t& n) {
    static const JosaPair k[] = {
        {L"이야말로", L"야말로"}, {L"이라고", L"라고"}, {L"이라는", L"라는"}, {L"이라서", L"라서"}, {L"이라면", L"라면"},
        {L"이라도", L"라도"}, {L"이든지", L"든지"}, {L"이나마", L"나마"}, {L"이에요", L"예요"}, {L"이었", L"였"},
        {L"이랑", L"랑"}, {L"이란", L"란"}, {L"이며", L"며"}, {L"이든", L"든"}, {L"이나", L"나"}, {L"이여", L"여"},
        {L"은", L"는"}, {L"을", L"를"}, {L"과", L"와"},
    };
    n = sizeof(k) / sizeof(k[0]);
    return k;
}
// 받침 있는 말 뒤에서만 바꾸는 서술격 꼴 (유나다 → 서윤이다 쪽은 "유나다운"처럼 다른 말일 수 있어 바꾸지 않음)
inline const JosaPair* CopulaPairs(size_t& n) {
    static const JosaPair k[] = {
        {L"이잖아", L"잖아"}, {L"이니까", L"니까"}, {L"이다", L"다"}, {L"이지", L"지"}, {L"이네", L"네"}, {L"이고", L"고"},
        {L"이면", L"면"}, {L"이냐", L"냐"},
    };
    n = sizeof(k) / sizeof(k[0]);
    return k;
}
// 받침 있는 이름 뒤의 "이"(서윤이는, 서윤이가)에 이어지는 조사의 첫 글자
inline bool AfterHypoI(const std::wstring& t, size_t p) {
    if (p >= t.size()) return false;
    static const wchar_t* follow[] = {L"는", L"가", L"를", L"도", L"의", L"랑", L"한테", L"에게", L"에", L"와", L"만", L"처럼", L"보다",
                                      L"까지", L"부터", L"조차", L"마저", L"께", L"하고", L"네"};
    for (const wchar_t* f : follow) if (At(t, p, f)) return true;
    return false;
}
// 받침 꼴(은·을·과·이…)은 받침 있는 말 뒤에서만, 받침 없는 꼴(는·를·와·가…)은 받침 없는 말 뒤에서만 조사로 봄.
// (찾을 말의 받침과 맞지 않는 글자는 조사가 아니라 다른 낱말: "나" 뒤의 "이가"는 "나이가")
// 이름 바로 뒤(pos)에 조사가 붙어 있는지 ("온전한 단어만"과 함께 쓸 때 조사는 단어 경계로 봄). oldLast: 찾을 말의 끝 글자
inline bool ParticleAt(const std::wstring& t, size_t pos, wchar_t oldLast) {
    if (pos >= t.size() || !Syl(t[pos]) || !Syl(oldLast)) return false;
    const bool oldB = Jong(oldLast) > 0, oldR = Jong(oldLast) == 8;
    size_t n;
    const JosaPair* k = JosaPairs(n);
    for (size_t i = 0; i < n; ++i) {
        const wchar_t* f = oldB ? k[i].c : k[i].v;
        if (At(t, pos, f) && JosaTailOk(t, pos + Len(f))) return true;
    }
    if (oldB) { k = CopulaPairs(n); for (size_t i = 0; i < n; ++i) if (At(t, pos, k[i].c)) return true; }
    if (At(t, pos, (oldB && !oldR) ? L"으로" : L"로")) return true;
    if (At(t, pos, oldB ? L"이" : L"가") || At(t, pos, oldB ? L"아" : L"야")) return true;
    if (!oldB && (At(t, pos, L"였") || At(t, pos, L"예요"))) return true;
    for (const wchar_t* w : {L"의", L"도", L"만", L"에게", L"에서", L"에", L"한테", L"께", L"처럼", L"보다", L"까지", L"부터", L"조차",
                             L"마저", L"하고", L"밖에", L"네"})
        if (At(t, pos, w)) return true;
    return false;
}
// 이름 바로 뒤(pos)의 조사를 새 이름 끝 글자(newLast)의 받침에 맞춤. oldLast: 찾을 말의 끝 글자.
// 바꿔야 하면 t[pos .. pos+반환값)을 part로 바꾸면 됨. 바꿀 것이 없으면 0.
inline size_t FixJosa(const std::wstring& t, size_t pos, wchar_t oldLast, wchar_t newLast, std::wstring& part) {
    part.clear();
    if (!Syl(newLast) || !Syl(oldLast) || pos >= t.size() || !Syl(t[pos])) return 0;
    const int jg = Jong(newLast);
    const bool hasB = jg > 0, rieul = jg == 8;
    const bool oldB = Jong(oldLast) > 0, oldR = Jong(oldLast) == 8;  // 지금 글에 쓰인 꼴은 찾을 말의 받침에 맞는 꼴이어야 조사
    auto put = [&](size_t len, const wchar_t* want, const std::wstring& rest) -> size_t {
        std::wstring w = std::wstring(want) + rest;
        if (t.compare(pos, len, w) == 0) return 0;  // 이미 맞음
        part = w;
        return len;
    };
    // 1) 으로 / 로 (ㄹ 받침 뒤는 "로")
    for (const wchar_t* tail : {L"부터", L"서", L"써", L""}) {
        std::wstring cur = std::wstring((oldB && !oldR) ? L"으로" : L"로") + tail;
        if (At(t, pos, cur.c_str())) return put(cur.size(), (hasB && !rieul) ? L"으로" : L"로", tail);
    }
    size_t n;
    const JosaPair* k = JosaPairs(n);
    if (oldB) {
    // 2) 받침 뒤 꼴로 쓰여 있음 (서윤은, 서윤이라고, 서윤이었다)
    for (size_t i = 0; i < n; ++i) {
        size_t L = Len(k[i].c);
        if (At(t, pos, k[i].c) && (L > 1 || JosaTailOk(t, pos + L))) return put(L, hasB ? k[i].c : k[i].v, L"");
    }
    if (At(t, pos, L"이야")) return put(2, hasB ? L"이야" : L"야", L"");
    const JosaPair* cp = CopulaPairs(n);
    for (size_t i = 0; i < n; ++i)
        if (At(t, pos, cp[i].c)) return put(Len(cp[i].c), hasB ? cp[i].c : cp[i].v, L"");
    if (t[pos] == L'이') {
        if (AfterHypoI(t, pos + 1)) return hasB ? 0 : put(1, L"", L"");          // 서윤이는 → 유나는
        if (pos + 1 >= t.size() || !Syl(t[pos + 1])) return put(1, hasB ? L"이" : L"가", L"");  // 서윤이 웃었다 → 유나가
        return 0;
    }
    if (t[pos] == L'아' && (pos + 1 >= t.size() || !Syl(t[pos + 1]))) return put(1, hasB ? L"아" : L"야", L"");  // 부르는 말
    return 0;
    }
    // 3) 받침 없는 말 뒤 꼴로 쓰여 있음 (유나는, 유나라고, 유나였다)
    k = JosaPairs(n);
    for (size_t i = 0; i < n; ++i) {
        size_t L = Len(k[i].v);
        bool past = k[i].v[0] == L'였';  // 유나였다·유나였고: 뒤에 늘 어미가 붙음
        if (At(t, pos, k[i].v) && (L > 1 || past || JosaTailOk(t, pos + L))) return put(L, hasB ? k[i].c : k[i].v, L"");
    }
    if (t[pos] == L'가' && JosaTailOk(t, pos + 1)) return put(1, hasB ? L"이" : L"가", L"");
    if (t[pos] == L'야' && (pos + 1 >= t.size() || !Syl(t[pos + 1]))) {
        if (!hasB) return 0;
        // 유나야, / 유나야! → 서윤아 (부르는 말)   ·   유나야. / 유나야? → 서윤이야 (…이다)
        wchar_t nx = pos + 1 < t.size() ? t[pos + 1] : L'\r';
        bool copula = In(nx, L".?…\"”'’」』");
        return put(1, copula ? L"이야" : L"아", L"");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 목차: 회차 번호 확인
inline bool TDigit(char32_t c) { return (c >= '0' && c <= '9') || (c >= 0xFF10 && c <= 0xFF19); }
inline int TDigitVal(char32_t c) { return c <= '9' ? (int)(c - '0') : (int)(c - 0xFF10); }
// 목차 제목 줄에서 회차 번호와 단위. 번호가 없으면 -1
// 제12화 / 12화 / [제3화] / 第5章 / 12話 / Chapter 7 / EP.3 / Episode 4 / #12
inline long TocNumber(const std::u32string& line, char32_t& unit) {
    unit = 0;
    size_t i = 0, n = line.size();
    auto sp = [&](char32_t c) { return c == ' ' || c == '\t' || c == 0x3000 || c == 0xA0; };
    bool hash = false;
    for (int k = 0; i < n && k < 8; ++k) {
        char32_t c = line[i];
        if (c == '#' || c == 0xFF03) hash = true;
        if (sp(c) || c == '[' || c == 0x3010 || c == '<' || c == 0x3008 || c == 0x300A || c == 0x300C || c == 0x300E || c == '(' ||
            c == 0xFF08 || c == '#' || c == 0xFF03 || c == 0x25C6 || c == 0x2605 || c == 0x25A0 || c == 0x203B || c == '-' || c == '=' || c == '*') { ++i; continue; }
        break;
    }
    auto lowerAt = [&](size_t p, const char* w) {
        for (; *w; ++w, ++p) {
            if (p >= n) return false;
            char32_t c = line[p];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != (char32_t)*w) return false;
        }
        return true;
    };
    size_t j = i;
    char32_t engUnit = 0;
    for (const char* w : {"chapter", "episode", "chap.", "ch.", "ep.", "ep"}) {
        if (lowerAt(i, w)) { j = i + std::char_traits<char>::length(w); engUnit = 'C'; break; }
    }
    if (engUnit) { while (j < n && (sp(line[j]) || line[j] == '.')) ++j; }
    else if (j < n && (line[j] == U'제' || line[j] == U'第')) { ++j; while (j < n && sp(line[j])) ++j; }
    size_t d = j;
    long v = 0;
    while (j < n && TDigit(line[j]) && j - d < 7) { v = v * 10 + TDigitVal(line[j]); ++j; }
    if (j == d) return -1;
    if (engUnit) { unit = 'C'; return v; }
    while (j < n && sp(line[j])) ++j;
    if (hash && j < n && !(line[j] == U'화' || line[j] == U'話' || line[j] == U'话' || line[j] == U'장' || line[j] == U'章' || line[j] == U'回')) { unit = '#'; return v; }
    if (j >= n) { if (hash) { unit = '#'; return v; } return -1; }
    char32_t u = line[j];
    if (u == U'話' || u == U'话') u = U'화';
    if (u == U'화' || u == U'回' || u == U'章' || u == U'장' || u == U'편' || u == U'부' || u == U'권') { unit = u; return v; }
    return -1;
}
struct TocCheck {
    std::vector<int> dup;                                   // 목차 항목 번호: 앞에 같은 번호가 이미 있음
    std::vector<std::pair<int, std::pair<long, long>>> gap; // (이 항목 앞에, 빠진 번호 from~to)
    int counted = 0;                                        // 번호로 확인한 항목 수
    long missingTotal = 0;
};
// nums[k]: k번째 목차 항목의 번호(-1 없음), units[k]: 단위. 가장 많이 쓴 단위의 항목만 봄
// 번호가 크게 줄면(새 권·새 부) 새로 셈, 50 넘게 뛰면 다른 묶음으로 보고 빠짐으로 치지 않음
inline TocCheck CheckTocNumbers(const std::vector<long>& nums, const std::vector<char32_t>& units) {
    TocCheck r;
    std::vector<std::pair<char32_t, int>> cnt;
    for (size_t k = 0; k < nums.size(); ++k) {
        if (nums[k] < 0) continue;
        bool f = false;
        for (auto& c : cnt) if (c.first == units[k]) { c.second++; f = true; }
        if (!f) cnt.push_back({units[k], 1});
    }
    char32_t best = 0; int bn = 0;
    for (auto& c : cnt) if (c.second > bn) { bn = c.second; best = c.first; }
    if (bn < 5) return r;
    r.counted = bn;
    long prev = -1;
    std::vector<long> seen;  // 지금 묶음에서 나온 번호
    for (size_t k = 0; k < nums.size(); ++k) {
        if (nums[k] < 0 || units[k] != best) continue;
        long v = nums[k];
        if (prev >= 0 && v < prev && v <= 1) { seen.clear(); seen.push_back(v); prev = v; continue; }  // 새 권·새 부: 0·1화부터 다시
        bool inSeries = false;
        for (long s : seen) if (s == v) { inSeries = true; break; }
        if (prev >= 0 && inSeries) { r.dup.push_back((int)k); continue; }
        if (prev >= 0 && v > prev + 1 && v - prev <= 50) {
            r.gap.push_back({(int)k, {prev + 1, v - 1}});
            r.missingTotal += v - prev - 1;
        } else if (prev >= 0 && (v < prev || v - prev > 50)) {
            seen.clear();  // 새 묶음
        }
        seen.push_back(v);
        prev = v;
    }
    return r;
}

// ---------------------------------------------------------------------------
// 정리 모드: 바뀐 곳 표시
inline bool ChgSpace(char32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x3000 || c == 0xA0 || c == 0x2028 || c == 0x2029 || c == 0x85 || c == 0x0C;
}
inline bool ChgInvisible(char32_t u) {
    return u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF || u == 0x2060 || u == 0xAD || u == 0x180E ||
           u == 0x200E || u == 0x200F || (u >= 0x202A && u <= 0x202E) || (u >= 0x2061 && u <= 0x2064) ||
           (u >= 0x2066 && u <= 0x2069) || (u >= 0xFE00 && u <= 0xFE0F);
}
inline bool ChgSkip(char32_t c) { return ChgSpace(c) || ChgInvisible(c) || c < 0x20; }
typedef std::vector<std::pair<size_t, size_t>> Runs;
inline void AddRun(Runs& runs, size_t i) {
    if (!runs.empty() && runs.back().second == i) runs.back().second = i + 1;
    else runs.push_back({i, i + 1});
}
// 결과 out[lo,hi)에서 원문과 달라진 글자(따옴표·말줄임표 바꿈, 새로 넣은 글자 등)의 구간. 공백·줄바꿈은 셈하지 않음
inline void ChangedRuns(const std::u32string& src, const std::u32string& out, const std::vector<int>& outToSrc,
                        size_t lo, size_t hi, Runs& runs, size_t maxRuns) {
    runs.clear();
    if (hi > out.size()) hi = out.size();
    if (hi > outToSrc.size()) hi = outToSrc.size();
    for (size_t j = lo; j < hi && runs.size() < maxRuns; ++j) {
        char32_t c = out[j];
        if (ChgSkip(c)) continue;
        int s = outToSrc[j];
        if (s < 0 || (size_t)s >= src.size() || src[s] != c) AddRun(runs, j);
    }
}
// 원문 글자 가운데 결과에 남은 것 (결과 글자가 가리키는 원문 위치)
inline void CoveredMap(size_t srcSize, const std::vector<int>& outToSrc, std::vector<char>& cov) {
    cov.assign(srcSize, 0);
    for (int s : outToSrc) if (s >= 0 && (size_t)s < srcSize) cov[s] = 1;
}
// 원문 src[lo,hi)에서 정리하며 지운 글자의 구간 (쪽 번호·광고 줄·HTML 찌꺼기·괄호 한자 등). 공백·줄바꿈·합쳐진 자모는 셈하지 않음
inline void DeletedRuns(const std::u32string& src, const std::vector<char>& cov, size_t lo, size_t hi, Runs& runs, size_t maxRuns) {
    runs.clear();
    if (hi > src.size()) hi = src.size();
    if (hi > cov.size()) hi = cov.size();
    for (size_t i = lo; i < hi && runs.size() < maxRuns; ++i) {
        char32_t c = src[i];
        if (cov[i] || ChgSkip(c) || (c >= 0x1160 && c <= 0x11FF)) continue;
        AddRun(runs, i);
    }
}

}  // namespace rt
