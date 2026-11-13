#!/usr/bin/env python3
"""src/reader_tools.h 시험 (조사 맞춤 · 목차 회차 번호 확인 · 바뀐 곳 표시 구간).

헤더를 그대로 g++로 컴파일해 실제 코드로 확인합니다. 정리 엔진과는 관계없습니다.
  python3 tests/check_reader_tools.py
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

# (원문, 찾을 말, 바꿀 말, 기대 결과) — 바꾸기 창의 "조사 맞춤"과 같은 방식으로 모두 바꾸기
JOSA = [
    ("서윤은 웃었다.", "서윤", "유나", "유나는 웃었다."),
    ("서윤을 보았다.", "서윤", "유나", "유나를 보았다."),
    ("서윤과 함께", "서윤", "유나", "유나와 함께"),
    ("서윤과의 약속", "서윤", "유나", "유나와의 약속"),
    ("서윤이 웃었다.", "서윤", "유나", "유나가 웃었다."),
    ("서윤이는 웃었다.", "서윤", "유나", "유나는 웃었다."),
    ("서윤이가 왔다.", "서윤", "유나", "유나가 왔다."),
    ("서윤이랑 놀았다.", "서윤", "유나", "유나랑 놀았다."),
    ("\"서윤아, 가자.\"", "서윤", "유나", "\"유나야, 가자.\""),
    ("서윤으로 정했다.", "서윤", "유나", "유나로 정했다."),
    ("서윤으로서 말한다.", "서윤", "유나", "유나로서 말한다."),
    ("서윤이라고 불렀다.", "서윤", "유나", "유나라고 불렀다."),
    ("서윤이었다.", "서윤", "유나", "유나였다."),
    ("범인은 서윤이다.", "서윤", "유나", "범인은 유나다."),
    ("서윤이에요.", "서윤", "유나", "유나예요."),
    ("서윤의 책", "서윤", "유나", "유나의 책"),
    ("서윤도 왔다.", "서윤", "유나", "유나도 왔다."),
    ("서윤에게 줬다.", "서윤", "유나", "유나에게 줬다."),
    # 받침 없는 이름 → 받침 있는 이름
    ("유나는 웃었다.", "유나", "서윤", "서윤은 웃었다."),
    ("유나를 보았다.", "유나", "서윤", "서윤을 보았다."),
    ("유나와 함께", "유나", "서윤", "서윤과 함께"),
    ("유나가 왔다.", "유나", "서윤", "서윤이 왔다."),
    ("유나로 정했다.", "유나", "서윤", "서윤으로 정했다."),
    ("유나라고 불렀다.", "유나", "서윤", "서윤이라고 불렀다."),
    ("유나였다.", "유나", "서윤", "서윤이었다."),
    ("유나예요.", "유나", "서윤", "서윤이에요."),
    ("유나랑 놀았다.", "유나", "서윤", "서윤이랑 놀았다."),
    ("\"유나야, 가자.\"", "유나", "서윤", "\"서윤아, 가자.\""),
    ("\"유나야!\"", "유나", "서윤", "\"서윤아!\""),
    ("범인은 유나야.", "유나", "서윤", "범인은 서윤이야."),
    ("유나다운 생각", "유나", "서윤", "서윤다운 생각"),   # 다른 말(…답다)은 그대로
    ("유나의 책", "유나", "서윤", "서윤의 책"),
    # ㄹ 받침: 으로가 아니라 로
    ("서윤으로 정했다.", "서윤", "하늘", "하늘로 정했다."),
    ("하늘로 정했다.", "하늘", "서윤", "서윤으로 정했다."),
    ("하늘은 맑다.", "하늘", "유나", "유나는 맑다."),
    # 받침 있는 → 받침 있는 (서윤이는 → 하준이는 처럼 "이"는 그대로)
    ("서윤이는 웃었다.", "서윤", "하준", "하준이는 웃었다."),
    ("서윤은 웃었다.", "서윤", "하준", "하준은 웃었다."),
    # 조사가 아닌 말이 이어지면 건드리지 않음
    ("서윤이름표", "서윤", "유나", "유나이름표"),
    ("유나가족", "유나", "서윤", "서윤가족"),
    # 새 이름 끝이 한글이 아니면 조사는 그대로
    ("서윤은 웃었다.", "서윤", "Mary", "Mary은 웃었다."),
    # 여러 곳
    ("서윤은 서윤을 보고 서윤과 웃었다.", "서윤", "유나", "유나는 유나를 보고 유나와 웃었다."),
]

# (목차 줄들, 기대: 빠진 번호 구간 문자열, 겹친 항목 번호)
TOC = [
    (["프롤로그", "1화", "2화", "3화", "5화", "6화", "8화"], "4-4,7-7", []),
    (["제1화 시작", "제2화", "제3화", "제3화", "제4화", "제5화"], "", [3]),
    (["1화", "2화", "3화", "4화", "5화", "1화", "2화", "4화", "5화", "6화"], "3-3", []),   # 새 권에서 다시 1화
    (["Chapter 1", "Chapter 2", "Chapter 4", "Chapter 5", "Chapter 6"], "3-3", []),
    (["第1章", "第2章", "第3章", "第5章", "第6章"], "4-4", []),
    (["1화", "2화", "3화", "4화"], "", []),                      # 4개뿐: 확인하지 않음
    (["1화", "2화", "3화", "4화", "5화", "200화", "201화"], "", []),  # 크게 뛰면 다른 묶음
    (["【1話】", "【2話】", "【3話】", "【4話】", "【6話】"], "5-5", []),
]

HARNESS = r"""
#include "%s"
#include <cstdio>
#include <string>
#include <vector>
using namespace std;
static wstring Dec(const string& s) {
    wstring r; size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i]; unsigned u; int n;
        if (c < 0x80) { u = c; n = 1; } else if ((c >> 5) == 6) { u = c & 31; n = 2; } else if ((c >> 4) == 14) { u = c & 15; n = 3; } else { u = c & 7; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); ++k) u = (u << 6) | (s[i + k] & 63);
        r += (wchar_t)u; i += n;
    }
    return r;
}
static string Enc(const wstring& w) {
    string r;
    for (wchar_t wc : w) { unsigned u = (unsigned)wc;
        if (u < 0x80) r += char(u); else if (u < 0x800) { r += char(0xC0 | (u >> 6)); r += char(0x80 | (u & 63)); }
        else { r += char(0xE0 | (u >> 12)); r += char(0x80 | ((u >> 6) & 63)); r += char(0x80 | (u & 63)); } }
    return r;
}
static wstring ReplaceAll(const wstring& t, const wstring& q, const wstring& rep) {
    wstring out; size_t at = 0, pos = 0;
    while ((pos = t.find(q, pos)) != wstring::npos) {
        size_t e = pos + q.size();
        wstring part; size_t used = rt::FixJosa(t, e, rep.empty() ? 0 : rep.back(), part);
        out.append(t, at, pos - at); out += rep; out += used ? part : wstring();
        at = e + used; pos = at;
    }
    out.append(t, at, wstring::npos);
    return out;
}
int main() {
    char buf[4096];
    int mode = 0;
    // 입력: 줄마다 "J\t원문\t찾을\t바꿀" 또는 "T\t줄|줄|줄"
    while (fgets(buf, sizeof buf, stdin)) {
        string l(buf); while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        vector<string> f; size_t a = 0;
        for (;;) { size_t b = l.find('\t', a); f.push_back(l.substr(a, b == string::npos ? string::npos : b - a)); if (b == string::npos) break; a = b + 1; }
        if (f[0] == "J") { printf("%%s\n", Enc(ReplaceAll(Dec(f[1]), Dec(f[2]), Dec(f[3]))).c_str()); }
        else if (f[0] == "T") {
            vector<long> nums; vector<char32_t> units; size_t p = 0; string s = f[1];
            for (;;) { size_t b = s.find('|', p); wstring w = Dec(s.substr(p, b == string::npos ? string::npos : b - p));
                u32string u(w.begin(), w.end()); char32_t un; nums.push_back(rt::TocNumber(u, un)); units.push_back(un);
                if (b == string::npos) { break; }
                p = b + 1; }
            rt::TocCheck r = rt::CheckTocNumbers(nums, units);
            string g;
            for (auto& x : r.gap) { if (!g.empty()) g += ","; g += to_string(x.second.first) + "-" + to_string(x.second.second); }
            string d; for (int k : r.dup) { if (!d.empty()) d += ","; d += to_string(k); }
            printf("%%s;%%s\n", g.c_str(), d.c_str());
        }
    }
    (void)mode;
    // 바뀐 곳 구간
    {
        u32string src = U"\"가나\" 다라.\n- 12 -\n마바";
        u32string out = U"“가나” 다라.\n\n마바\n";
        vector<int> m;  // out -> src
        int map[] = {0,1,2,3,4,5,6,7,8,8,16,17,17};
        for (int v : map) m.push_back(v);
        rt::Runs runs; rt::ChangedRuns(src, out, m, 0, out.size(), runs, 100);
        if (!(runs.size() == 2 && runs[0] == make_pair((size_t)0, (size_t)1) && runs[1] == make_pair((size_t)3, (size_t)4))) { printf("CHG FAIL\n"); return 1; }
        vector<char> cov; rt::CoveredMap(src.size(), m, cov);
        rt::DeletedRuns(src, cov, 0, src.size(), runs, 100);
        // "- 12 -" (위치 9~14) 중 공백이 아닌 글자: '-'(9) '12'(11,12) '-'(14)
        if (!(runs.size() == 3 && runs[0].first == 9 && runs[1] == make_pair((size_t)11, (size_t)13) && runs[2].first == 14)) { printf("DEL FAIL %%zu\n", runs.size()); return 1; }
        printf("RUNS OK\n");
    }
    return 0;
}
"""


def main():
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / "rt.cpp"
        exe = Path(td) / "rt"
        cpp.write_text(HARNESS % (root / "src" / "reader_tools.h").as_posix(), encoding="utf-8")
        subprocess.run(["g++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
                        str(cpp), "-o", str(exe)], check=True)
        lines = [f"J\t{a}\t{b}\t{c}" for a, b, c, _ in JOSA] + ["T\t" + "|".join(t) for t, _, _ in TOC]
        out = subprocess.run([str(exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, encoding="utf-8", check=True).stdout.splitlines()
        fails = 0
        for (src, a, b, want), got in zip(JOSA, out):
            if got != want:
                fails += 1
                print(f"[조사 실패] {src}  ({a}→{b})\n   기대: {want}\n   결과: {got}")
        for (toc, gaps, dups), got in zip(TOC, out[len(JOSA):]):
            g, d = got.split(";")
            want_d = ",".join(str(x) for x in dups)
            if g != gaps or d != want_d:
                fails += 1
                print(f"[목차 실패] {toc}\n   기대: 빠짐 {gaps!r} 겹침 {want_d!r}\n   결과: 빠짐 {g!r} 겹침 {d!r}")
        if out[-1] != "RUNS OK":
            fails += 1
            print("[바뀐 곳 구간 실패]", out[-1])
        print(f"조사 맞춤 {len(JOSA)}개 · 목차 번호 {len(TOC)}개 · 바뀐 곳 구간 시험: 실패 {fails}개")
        return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())
