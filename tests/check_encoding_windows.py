#!/usr/bin/env python3
"""Encoding detection: compile the REAL decoding code of src/main.cpp and of the v2.4.4
release source (commit f2a6786c) side by side, and run both on a matrix of files.

* Cases the old code already read correctly (Korean in every encoding, Japanese Shift-JIS,
  Chinese GBK/Big5, UTF-8/16, European and other scripts) must give exactly the same
  encoding and text with the new code.
* The cases v2.4.4 broke (EUC-JP, GB18030 with symbols outside GBK, Big5 with extended
  characters, UTF-8 cut at fixed byte counts, BOM-less UTF-16 Chinese/Japanese) must now be
  read correctly.

Runs natively on Windows (cl). Elsewhere it uses MinGW + Wine when both are present.
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
REF = "f2a6786c706b6cd7c7f66c87fa46878230e5dfb4"   # v2.4.4 as first released


def decoding_source(text: str) -> str:
    head = text[text.index("enum { E_UTF8,"):text.index("static UINT EncCp(int enc)")]
    enc_cp = text[text.index("static UINT EncCp(int enc)"):]
    enc_cp = enc_cp[:enc_cp.index("\n}\n") + 3]
    body = text[text.index("// 문자 변환"):text.index("static std::string Encode(")]
    return head + enc_cp + body


HARNESS = r"""
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cwchar>
using std::wstring;
typedef std::u32string U32;
%SRC%
int wmain(int argc, wchar_t** argv) {
    // argv: in-file out-file ; writes "<enc>\n" + text as UTF-8
    FILE* f = _wfopen(argv[1], L"rb"); std::string b;
    char buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) b.append(buf, k);
    fclose(f);
    int enc = -1;
    U32 t = Decode(b, enc, 0);
    std::string out = std::to_string(enc) + "\n";
    for (char32_t c : t) {
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
        else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    }
    FILE* o = _wfopen(argv[2], L"wb"); fwrite(out.data(), 1, out.size(), o); fclose(o);
    return 0;
}
"""
# The harness reads many files in one process run: a list file, one in/out pair per line.
HARNESS = HARNESS.replace("int wmain(int argc, wchar_t** argv) {", "static int one(wchar_t** argv) {") + r"""
int wmain(int argc, wchar_t** argv) {
    FILE* l = _wfopen(argv[1], L"rb"); std::string s; char buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof buf, l)) > 0) s.append(buf, k);
    fclose(l);
    size_t p = 0;
    while (p < s.size()) {
        size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size();
        std::string line = s.substr(p, e - p); p = e + 1;
        size_t tab = line.find('\t'); if (tab == std::string::npos) continue;
        wstring a(line.begin(), line.begin() + tab), b2(line.begin() + tab + 1, line.end());
        wchar_t* args[3] = {nullptr, (wchar_t*)a.c_str(), (wchar_t*)b2.c_str()};
        one(args);
    }
    return 0;
}
"""
E = ['UTF8', 'UTF8BOM', 'CP949', 'UTF16LE', 'UTF16BE', 'SJIS', 'GBK', 'BIG5', 'W1252', 'JOHAB',
     'W1251', 'KOI8R', 'W1250', 'W1253', 'W1254', 'W1255', 'W1256', 'W1257', 'W1258', 'W874', 'EUCJP']


def build(src_text: str, work: Path, name: str) -> list[str]:
    cpp = work / f"{name}.cpp"
    cpp.write_text(HARNESS.replace("%SRC%", decoding_source(src_text)), encoding="utf-8")
    if os.name == "nt":
        exe = work / f"{name}.exe"
        r = subprocess.run(["cl", "/nologo", "/std:c++17", "/EHsc", "/utf-8", "/O2", str(cpp), "/Fe:" + str(exe), "user32.lib"],
                           cwd=work, capture_output=True, text=True, errors="replace")
        if r.returncode:
            raise RuntimeError(f"{name}: cl failed\n" + "\n".join(l for l in r.stdout.splitlines() if "error" in l)[:3000])
        return [str(exe)]
    exe = work / f"{name}.exe"
    subprocess.run(["x86_64-w64-mingw32-g++-win32", "-w", "-O2", "-std=c++17", "-municode", "-static", str(cpp), "-o", str(exe)],
                   check=True)
    return ["wine", str(exe)]


# ---------------------------------------------------------------- texts
KO = (root / "dist/samples/sample_korean_special.txt").read_text(encoding="utf-8-sig") + "\n" + \
     (root / "dist/samples/sample_mobile_fixedwidth.txt").read_bytes().decode("cp949") + "\n" + \
     (root / "dist/samples/sample_messy.txt").read_text(encoding="utf-8-sig")
KO_CHAT = "ㅋㅋㅋㅋㅋ 진짜 웃기다 ㅠㅠ\nㅎㅎ 그래 ㅋㅋㅋ\n" * 40 + "ㄱㄴㄷㄹ ㅏㅑㅓㅕ\n" * 10
KO_HANJA = "大韓民國 憲法 第一條 大韓民國은 民主共和國이다. 大韓民國의 主權은 國民에게 있고, 모든 權力은 國民으로부터 나온다.\n" * 20
ZHS = """第一章 初遇
那一年的冬天特别冷。林雨站在车站的出口，看着漫天飞舞的雪花，心里却是一片茫然。她已经在这个城市里生活了三年，可是直到今天，她才第一次觉得自己真的是一个人。
“你是在等人吗？”一个低沉的声音从身后传来。她回过头，看见一个穿着黑色大衣的男人，正微笑着看着她。
“不是，我只是在想事情。”她轻声说道，下意识地把手插进了口袋里。
男人点了点头，没有再说什么，只是站在她的身边，和她一起看着外面的雪。过了很久，他才开口说：“我叫陈屿，住在前面那条街上。如果你没有地方去的话，可以先到我那里坐一坐，喝杯热茶。”
林雨看了他一眼，犹豫了一下，最后还是跟着他走了。她不知道这个决定会改变她以后所有的生活。"""
ZHT = """第一章 初遇
那一年的冬天特別冷。林雨站在車站的出口，看著漫天飛舞的雪花，心裡卻是一片茫然。她已經在這個城市裡生活了三年，可是直到今天，她才第一次覺得自己真的是一個人。
「你是在等人嗎？」一個低沉的聲音從身後傳來。她回過頭，看見一個穿著黑色大衣的男人，正微笑著看著她。
「不是，我只是在想事情。」她輕聲說道，下意識地把手插進了口袋裡。
男人點了點頭，沒有再說什麼，只是站在她的身邊，和她一起看著外面的雪。過了很久，他才開口說：「我叫陳嶼，住在前面那條街上。如果你沒有地方去的話，可以先到我那裡坐一坐，喝杯熱茶。」
林雨看了他一眼，猶豫了一下，最後還是跟著他走了。她不知道這個決定會改變她以後所有的生活。"""
JA = """第一章　出会い
その年の冬は特別に寒かった。駅の出口に立った彼女は、空一面に舞う雪を見上げながら、ぼんやりとしていた。この街に住んで三年になるが、今日初めて、自分は本当に一人なのだと感じた。
「誰かを待っているんですか」と、後ろから低い声が聞こえた。振り返ると、黒いコートを着た男がほほえみながらこちらを見ていた。
「いいえ、ちょっと考え事をしていただけです」と彼女は小さな声で答え、思わず手をポケットに入れた。
男はうなずいて、それ以上は何も言わず、ただ彼女の隣に立って一緒に外の雪を眺めていた。しばらくして、彼はようやく口を開いた。"""
JA_KANJI = "第一回　桃園結義\n話説天下大勢、分久必合、合久必分。周末七国分争、并入於秦。及秦滅之後、楚漢分争、又并入於漢。\n" * 8
EN = "It was a dark and stormy night; the rain fell in torrents, except at occasional intervals.\n" * 30
FR = "Il était une fois une très belle princesse qui habitait un château près de la forêt. Où était-elle allée ? Ça dépend.\n" * 20
RU = "Все счастливые семьи похожи друг на друга, каждая несчастливая семья несчастлива по-своему.\n" * 20
EL = "Ήταν μια σκοτεινή και θυελλώδης νύχτα, και η βροχή έπεφτε καταρρακτωδώς.\n" * 20


def cut_lines(data: bytes, width: int) -> bytes:
    return b"\r\n".join(data[i:i + width] for i in range(0, len(data), width))


def with_symbols(text: str, every: int) -> str:
    """The text six times, with symbols GBK lacks on every `every`-th line (as GB18030 web novels have)."""
    lines = ((text + "\n") * 6).split("\n")
    return "\n".join(l + ("❤✨" if i % every == 0 else "") for i, l in enumerate(lines))


# (name, bytes, expected enc names (old = what v2.4.4 gave, new = what is wanted), expected text or None)
same = []      # must behave exactly as v2.4.4
fixed = []     # v2.4.4 broke these; must now be read as `want`
def legacy(text: str, codec: str) -> bytes:
    """Only the characters the legacy code page has (the sample also holds decomposed jamo etc.)."""
    import unicodedata
    return "".join(c for c in unicodedata.normalize("NFC", text) if _enc_ok(c, codec)).encode(codec)


def _enc_ok(c, codec):
    try:
        c.encode(codec); return True
    except UnicodeEncodeError:
        return False


for name, t in (("ko", KO), ("ko-chat", KO_CHAT), ("ko-hanja", KO_HANJA)):
    same += [(f"{name}.cp949", legacy(t, "cp949")), (f"{name}.utf8", t.encode("utf-8")),
             (f"{name}.utf8bom", b"\xef\xbb\xbf" + t.encode("utf-8")), (f"{name}.utf16le", b"\xff\xfe" + t.encode("utf-16-le")),
             (f"{name}.utf16le-nobom", t.encode("utf-16-le")), (f"{name}.johab", legacy(t, "johab"))]
same += [("ko.cp949-cut", cut_lines(legacy(KO, "cp949"), 60))]
same += [("zhs.gbk", ZHS.encode("gbk")), ("zht.big5", ZHT.encode("big5")), ("zht-as-gbk", ZHT.encode("gbk")),
         ("ja.sjis", JA.encode("cp932")), ("ja-kanji.sjis", JA_KANJI.encode("cp932")),
         ("zhs.utf8", ZHS.encode("utf-8")), ("ja.utf8", JA.encode("utf-8")), ("zhs.utf16le", b"\xff\xfe" + ZHS.encode("utf-16-le")),
         ("en.ascii", EN.encode("ascii")), ("fr.1252", FR.encode("cp1252")), ("ru.1251", RU.encode("cp1251")),
         ("ru.koi8", RU.encode("koi8-r")), ("el.1253", EL.encode("cp1253")), ("fr.utf8", FR.encode("utf-8")),
         ("en.utf16le-nobom", EN.encode("utf-16-le")), ("short.gbk", ZHS.split("\n")[1][:40].encode("gbk")),
         ("short.sjis", JA.split("\n")[1][:30].encode("cp932"))]
fixed += [("ja.eucjp", JA.encode("euc_jp"), "EUCJP", JA),
          ("ja.eucjp-x6", (JA + "\n") .encode("euc_jp") * 6, "EUCJP", (JA + "\n") * 6),
          ("zhs.gb18030-sym", with_symbols(ZHS, 3).encode("gb18030"), "GBK", with_symbols(ZHS, 3) if os.name == "nt" else None),
          ("zhs.gb18030-1sym", with_symbols(ZHS, 60).encode("gb18030"), "GBK", with_symbols(ZHS, 60) if os.name == "nt" else None),
          ("zht.big5-ext", ((ZHT + "\n") * 6).replace("著", "着").encode("big5hkscs"), "BIG5", None),
          ("zhs.utf8-cut100", cut_lines(((ZHS + "\n") * 6).encode("utf-8"), 100), "UTF8", None),
          ("ja.utf8-cut100", cut_lines(((JA + "\n") * 6).encode("utf-8"), 100), "UTF8", None),
          ("ko.utf8-cut40", cut_lines(KO.encode("utf-8"), 40), "UTF8", None),   # v2.4.4 read this Korean UTF-8 as 서유럽
          ("zhs.utf16le-nobom", ZHS.encode("utf-16-le"), "UTF16LE", ZHS),
          ("ja.utf16le-nobom", JA.encode("utf-16-le"), "UTF16LE", JA),
          ("zht.utf16be-nobom", ZHT.encode("utf-16-be"), "UTF16BE", ZHT)]


def run(cmd, work, cases, tag):
    lst = work / f"list-{tag}.txt"
    lines = []
    for name, data, *_ in cases:
        (work / f"{name}.in").write_bytes(data)
        lines.append(f"{work / (name + '.in')}\t{work / (name + '.' + tag)}")
    lst.write_text("\n".join(lines), encoding="ascii")
    subprocess.run(cmd + [str(lst)], check=True, timeout=600, stderr=subprocess.DEVNULL)
    out = {}
    for name, *_ in cases:
        raw = (work / f"{name}.{tag}").read_bytes().decode("utf-8")
        enc, _, text = raw.partition("\n")
        out[name] = (E[int(enc)], text)
    return out


def main():
    if os.name != "nt" and not (shutil.which("x86_64-w64-mingw32-g++-win32") and shutil.which("wine")):
        print("SKIP: needs Windows (cl) or MinGW + Wine"); return
    new_src = (root / "src/main.cpp").read_text(encoding="utf-8")
    old_src = subprocess.check_output(["git", "show", f"{REF}:src/main.cpp"], cwd=root).decode("utf-8")
    with tempfile.TemporaryDirectory() as d:
        work = Path(d)
        new = build(new_src, work, "new"); old = build(old_src, work, "old")
        allc = same + [(n, b) for n, b, *_ in fixed]
        rn, ro = run(new, work, allc, "new"), run(old, work, allc, "old")
        bad = []
        for name, _ in same:
            if rn[name] != ro[name]:
                bad.append(f"changed: {name}: v2.4.4 {ro[name][0]} → now {rn[name][0]}")
        for name, data, want, text in fixed:
            enc, got = rn[name]
            ok = enc == want and (text is None or got.replace("\r\n", "\n").strip() == text.replace("\r\n", "\n").strip())
            if text is None and enc == want:
                # damaged / extended files: nearly every character right (only the damaged places differ)
                ref = {"UTF8": "utf-8", "BIG5": "big5hkscs", "GBK": "gb18030"}[want]   # (Wine has no GB18030: symbols differ)
                import difflib
                strip = lambda x: "".join(c for c in x if c not in "\ufffd\r\n?")
                ok = difflib.SequenceMatcher(None, strip(got), strip(data.decode(ref, errors="replace")), autojunk=False).ratio() >= 0.95
            print(f"{name:22s} v2.4.4={ro[name][0]:8s} now={enc:8s} {'OK' if ok else 'FAIL'}")
            if not ok:
                bad.append(f"not fixed: {name}: {enc}")
        for name, _ in same:
            print(f"{name:22s} {rn[name][0]:8s} same as v2.4.4")
        assert not bad, "\n".join(bad)
    print(f"ENCODING=PASS ({len(same)} unchanged, {len(fixed)} fixed)")


if __name__ == "__main__":
    try:
        main()
    except BaseException as e:   # GitHub Actions: show the reason as an annotation (job logs may be unreachable)
        if os.environ.get("GITHUB_ACTIONS"):
            msg = f"{type(e).__name__}: {e}".replace("%", "%25").replace("\r", "").replace("\n", "%0A")
            print(f"::error title=encoding test::{msg[:4000]}")
        raise
