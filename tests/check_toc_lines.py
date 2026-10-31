#!/usr/bin/env python3
"""읽기 모드 목차(IsChapterLine) 회귀 점검.

- src/main.cpp 의 목차 판별 함수를 떼어 g++ 로 컴파일해 실제 코드로 시험한다.
- v2.4.4(기준 커밋)에서 목차로 잡히던 줄은 지금도 모두 잡혀야 한다 (줄어들면 실패).
- 새로 지원하는 제목 형태는 잡혀야 하고, 본문 문장은 잡히면 안 된다.

사용: python3 tests/check_toc_lines.py [--base <git 커밋>] [글 파일 ...]
글 파일을 주면 줄 수·판별 수만 출력한다 (내용은 출력하지 않음).
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_DEFAULT = "a55f431"   # v2.4.4


def extract(src: str) -> str:
    m = re.search(r"static bool IsInvisibleCh\(char32_t u\) \{.*?\n\}\n", src, re.S)
    assert m, "IsInvisibleCh 없음"
    inv = m.group(0)
    a = src.index("static bool IsDigitCh(char32_t c)")
    b = src.index("static void ShowTocMenu(HWND anchor)")
    return inv + src[a:b]


def build(cur: str, old: str, d: str) -> str:
    code = r'''
#include <string>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
typedef std::u32string U32;
namespace cur {
%s
}
namespace old {
%s
}
static U32 Dec(const std::string& s) {
    U32 r; size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i]; char32_t u; int n;
        if (c < 0x80) { u = c; n = 1; } else if ((c >> 5) == 6) { u = c & 31; n = 2; }
        else if ((c >> 4) == 14) { u = c & 15; n = 3; } else { u = c & 7; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); ++k) u = (u << 6) | (s[i + k] & 63);
        r += u; i += n;
    }
    return r;
}
int main(int argc, char** argv) {
    // 입력: 파일, 줄마다 판별 결과 "old cur"
    std::ifstream f(argv[1], std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    U32 t = Dec(ss.str());
    size_t a = 0;
    for (size_t i = 0; i <= t.size(); ++i) {
        if (i == t.size() || t[i] == '\n') {
            bool o = i > a && old::IsChapterLine(t, a, i), c = i > a && cur::IsChapterLine(t, a, i);
            printf("%%d%%d\n", (int)o, (int)c);
            a = i + 1;
        }
    }
}
''' % (cur, old)
    cpp = os.path.join(d, "toc.cpp")
    exe = os.path.join(d, "toc")
    open(cpp, "w", encoding="utf-8").write(code)
    subprocess.run(["g++", "-std=c++17", "-O1", "-w", cpp, "-o", exe], check=True)
    return exe


def run(exe: str, d: str, lines):
    p = os.path.join(d, "in.txt")
    open(p, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    out = subprocess.run([exe, p], check=True, capture_output=True, text=True).stdout.split()
    return [(r[0] == "1", r[1] == "1") for r in out]


MUST = [
    # 예전부터 되던 것
    "제1화", "제 12 화 시작", "1화", "제3장", "[제5화]", "【第3話】", "Chapter 7", "프롤로그", "Epilogue", "第12章",
    # 한자 숫자 (이번 문의)
    "第一章 出发", "第十二章", "第一百零三章 再会", "第二十回 夜宴", "第三話", "第四话 归来", "第一卷 风起",
    "第二节 序", "第五集", "第一幕", "第二部 下山", "第十八章重逢", "　　第七章　远行",
    # 아라비아 숫자 + 중국어·일본어 단위
    "第3卷", "第2节 雨", "第12集", "第4话", "第2巻 旅立ち", "12話 帰還", "5回 対決",
    # 제로폭 글자·꾸밈
    "제​1화", "​第​一章", "◆ 제3화 ◆", "★第一章★", "※ 第二章", "- 제4화 -", "=== Chapter 3 ===",
    "《第一章》", "（第二章）", "■ 1화", "#12",
    # 영어
    "CHAPTER ONE", "Chapter Twenty-One", "chapter 4", "Chapter IV", "Ch. 12", "Chap. 3", "Part II", "PART 1",
    "Part One", "Episode 5", "EP 3", "Ep.7", "Vol. 2", "Volume 3", "Book One", "Chapter 3: The Gate",
    "Chapter 3 The Gate", "Part 2 - Winter",
    # 머리말·맺음말
    "序章", "楔子", "番外 一", "尾声", "后记", "プロローグ", "あとがき", "서막", "막간", "번외", "작가의 말",
    "Prologue", "INTERLUDE", "Afterword",
    # 긴 제목 (40자 넘음)
    "第十二章 " + "长" * 45, "제12화 " + "가" * 50,
]
MUST_NOT = [
    "我第一回见到她的时候，天正下着雨。", "第一节课是数学。", "第一部分的内容很简单。", "他第一次来这里。",
    "第一集团的总部在北京。", "第一季度的报表出来了。",
    "Part of the plan was simple.", "Part 2 of the plan failed.", "Book I bought yesterday was good.",
    "Chapters are short.", "The party started at nine.", "Episode of the week", "Volume up!",
    "- [제1화], 【2화】, <3화>, 《외전》처럼 괄호로 감싼 회차 표시도 알아봅니다.",
    "제1장, 그리고 제2장까지 읽고 나서야 그는 이 책이 무엇을 말하려는지 조금 알 것 같았다.",
    "1. 사과", "001", "그는 제일 먼저 왔다.", "삼화 아파트", "Chapter", "第", "제", "",
    "他说：“第一章写得不好。”我没回答，只是看着窗外的雨。他又说了一遍，声音很轻很轻很轻。",
]


def main():
    args = sys.argv[1:]
    base = BASE_DEFAULT
    if args[:1] == ["--base"]:
        base, args = args[1], args[2:]
    cur_src = open(os.path.join(ROOT, "src", "main.cpp"), encoding="utf-8").read()
    old_src = subprocess.run(["git", "-C", ROOT, "show", f"{base}:src/main.cpp"], check=True,
                             capture_output=True).stdout.decode("utf-8")
    fails = 0
    with tempfile.TemporaryDirectory() as d:
        exe = build(extract(cur_src), extract(old_src), d)
        for s, (o, c) in zip(MUST, run(exe, d, MUST)):
            if not c:
                print("FAIL 목차로 잡혀야 함:", repr(s)); fails += 1
            if o and not c:
                print("FAIL 예전엔 잡혔는데 빠짐:", repr(s)); fails += 1
        for s, (o, c) in zip(MUST_NOT, run(exe, d, MUST_NOT)):
            if c and not o:
                print("FAIL 본문인데 목차로 잡힘:", repr(s)); fails += 1
        # 예전 목차 ⊆ 새 목차: 저장소 안 글 파일 전체
        texts = []
        for dp, dn, fn in os.walk(ROOT):
            if ".git" in dp:
                continue
            texts += [os.path.join(dp, x) for x in fn if x.endswith((".txt", ".md"))]
        lost = total_old = added = 0
        for p in texts:
            try:
                lines = open(p, encoding="utf-8").read().split("\n")
            except UnicodeDecodeError:
                continue
            for o, c in run(exe, d, lines):
                total_old += o
                lost += o and not c
                added += c and not o
        print(f"저장소 글 {len(texts)}개: 예전 목차 {total_old}줄 중 빠진 줄 {lost}, 새로 잡힌 줄 {added}")
        if lost:
            fails += 1
        for p in args:
            raw = open(p, "rb").read()
            for enc in ("utf-8-sig", "cp949", "gb18030", "utf-16"):
                try:
                    txt = raw.decode(enc); break
                except UnicodeDecodeError:
                    continue
            lines = txt.replace("\r\n", "\n").split("\n")
            r = run(exe, d, lines)
            print(f"{os.path.basename(p)}: 줄 {len(lines)}, 예전 {sum(o for o, _ in r)}, 지금 {sum(c for _, c in r)}, "
                  f"빠진 줄 {sum(o and not c for o, c in r)}")
    print("OK" if not fails else f"실패 {fails}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
