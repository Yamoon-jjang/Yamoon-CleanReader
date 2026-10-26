#!/usr/bin/env python3
"""Differential simulation of the ACTUAL v2.4.2 and candidate v2.4.3 search sources.
Test both search results and reuse/invalidation with mock Win32 text controls.
No synthetic replacement of the search algorithm is permitted.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
base_sha = "02f93fbd66b7a1017da4d27033d44e41eb63566c"
baseline = subprocess.check_output(["git", "show", f"{base_sha}:src/main.cpp"], cwd=root, text=True, encoding="utf-8")
candidate = (root / "src/search_replace.inc").read_text(encoding="utf-8")
def extract_func(src, name):
    mark = "static " 
    import re
    pat = re.compile(r"(?m)^static\s+[\w:<>]+\s+" + re.escape(name) + r"\s*\(")
    found = pat.search(src)
    assert found, name
    start = found.start()
    opening = src.find("{", start)
    depth = 1
    end = opening + 1
    while depth and end < len(src):
        if src[end] == "{": depth += 1
        elif src[end] == "}": depth -= 1
        end += 1
    assert not depth, name
    return src[start:end] + "\n"

def sources(src):
    h = src.index("struct RpHit {")
    end = src.index("\n}", h)+2  # RpWordCh
    struct_and_word = src[h:end] + "\n"
    fold = extract_func(src, "RpFold")
    find = extract_func(src, "RpSearch")
    cache = src[src.index("static wstring g_rpCacheHay"):src.index("static void RpInfo(")]
    assert cache.count("static bool RpSearchCached(")==1
    extra = extract_func(src,"RpHitFrom") if "static size_t RpHitFrom(" in src else ""
    return struct_and_word + fold + extra + find + cache

prefix = r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cwctype>
#include <iostream>
#include <random>
#include <regex>
#include <string>
#include <vector>
using std::wstring;
using std::size_t;
using std::vector;
static int g_rpFind=11, g_rpFindX=12, g_rpRepl=13, g_rpReplX=14, g_rpWnd=15;
static const int ID_RP_CASE=21, ID_RP_WORD=22, ID_RP_REGEX=23;
static wstring query, replacement, document;
static bool opt_case=false, opt_word=false, opt_regex=false;
static unsigned long long g_rpTextSerial=1;
static size_t hay_reads=0;
static int GetDlgItem(int, int v) { return v; }
static bool Checked(int v) {
    return v == ID_RP_CASE ? opt_case : v == ID_RP_WORD ? opt_word : opt_regex;
}
static wstring RpGet(int v, int) {
    wstring s = v == g_rpFind ? query : replacement, o;
    for (size_t i=0;i<s.size();++i) {
        if(s[i] == L'\r' && i+1<s.size() && s[i+1]==L'\n') {o += L'\r';++i;}
        else if(s[i]==L'\n') o+=L'\r';
        else o+=s[i];
    }
    return o;
}
static wstring RpHay() {++hay_reads;return document;}
static bool IsInvisibleCh(char32_t u) {
    return u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF || u == 0x2060 || u == 0xAD || u == 0x180E ||
           u == 0x200E || u == 0x200F || (u >= 0x202A && u <= 0x202E) || (u >= 0x2061 && u <= 0x2064) ||
           (u >= 0x2066 && u <= 0x2069) || (u >= 0xFE00 && u <= 0xFE0F);
}
"""
tail = r"""
static int trials = 0;
template<typename A,typename B> void compare_hits(const A& a, const B& b) {
    if (a.size()!=b.size()) {
        std::wcerr << L"Hit count mismatch: " << a.size() << L" vs " << b.size() << L"\n";
        std::abort();
    }
    for (size_t i=0;i<a.size();++i)
        if(a[i].a!=b[i].a || a[i].b!=b[i].b || a[i].rep!=b[i].rep) {
            std::wcerr << L"Mismatch at hit " << i << L" (" << a[i].a << L", " << a[i].b << L") vs ("
                << b[i].a << L", " << b[i].b << L")\n";
            std::abort();
        }
}
template<class Hits> static wstring replace_all(const wstring& src,const Hits& hits) {
    wstring out;size_t at=0;
    for(const auto& h:hits) {
        assert(h.a>=at && h.b<=src.size() && h.b>h.a);
        out.append(src,at,h.a-at);out+=h.rep;at=h.b;
    }
    out.append(src,at,wstring::npos);return out;
}
static void one(const wstring& src, const wstring& q, const wstring& r, bool cs,bool word,bool rx) {
    document=src;query=q;replacement=r;opt_case=cs;opt_word=word;opt_regex=rx;
    old::g_rpCacheValid = false; neo::g_rpCacheValid = false;
    ++g_rpTextSerial;
    wstring er1,er2;
    auto ok1=old::RpSearchCached(er1);
    auto ok2=neo::RpSearchCached(er2);
    if(ok1!=ok2 || er1!=er2) {std::wcerr << L"Error result mismatch\n";std::abort();}
    compare_hits(old::g_rpCacheHits,neo::g_rpCacheHits);
    if(ok1 && !old::g_rpCacheHits.empty())
       assert(replace_all(document,old::g_rpCacheHits)==replace_all(document,neo::g_rpCacheHits));
    // Change ONLY replacement, exercise the new candidate cache reuse.
    for(const auto& x : {wstring(L"new"),wstring(L"$1-$2"),wstring(L"\u200Dtest"),wstring(L"")}) {
        replacement=x;
        ok1=old::RpSearchCached(er1);
        ok2=neo::RpSearchCached(er2);
        if(ok1!=ok2 || er1!=er2) {std::wcerr << L"Error update mismatch\n";std::abort();}
        compare_hits(old::g_rpCacheHits,neo::g_rpCacheHits);
        if(ok1 && !old::g_rpCacheHits.empty())
            assert(replace_all(document,old::g_rpCacheHits)==replace_all(document,neo::g_rpCacheHits));
    }
    ++trials;
}
int main() {
  std::vector<wstring> strings={
    L"",L"ABC abc ABC", L"가나다 가나다 가나",L"안녕\u200B하세요 안녕하세요",
    L"ABC\rDEF\r",L"안녕\r\r你好\rこんにちは\rabc",L"ab1ab2ab3",
    L"A\rB\rC",L"a b c a b c",L"foo_bar foobar foo-bar foo",
    L"😀 😁 abc 你好",L"abc\\ndef",L"x\u200dy x\u200by",L"\r\r\r",
    L"a\tb\ta",L"one two three",L"abcabcabc"
  };
  std::vector<wstring> queries={
    L"a",L"ABC",L"가나",L"안녕",L"foo",L"ab",L"abc",L"\\w+",L"^abc",L"abc$",
    L"(a)(b)",L"(abc)",L"\\babc\\b",L"[a-z]+",L"(foo|bar)",L".*",L"a.*b",
    L"(\\w+)-(\\w+)",L"(a+)+b",L"\\n",L"a\\nb",L"(",L".{0,3}",
    L"\u200B", L"\r"
  };
  for(auto& doc:strings)
    for(auto& q:queries)
      for(bool regex:{false,true})
        for(bool word:{false,true})
          for(bool cs:{false,true})
            one(doc,q,L"변환$1",cs,word,regex);
  std::mt19937 rng(0x243);
  const wstring chars=L"abcXYZ123 가나다中文\u200B\u200D\r  -_#$";
  for(int t=0;t<1100;++t) {
      size_t n = 10+rng()%250;
      wstring doc;for(size_t i=0;i<n;++i) doc+=chars[rng()%chars.size()];
      if(t%55==0)doc+=wstring(4100,L'a')+L" " + wstring(4100,L'b') + L"\r"+L"aaab";
      const wstring& q = queries[rng()%queries.size()];
      one(doc,q,L"$1/$2\\n\nDone",bool(rng()%2),bool(rng()%2),bool(rng()%2));
  }
  for(const wstring& q:{L"^ab",L"ab$",L"(ab)+",L"\\bfoo\\b",L"(foo)\\n(bar)",L".*ab"}) {
      auto doc=wstring(4048,L'x')+L" ab ab foo\rbar\r" + wstring(4000,L'a')+L" ab";
      one(doc,q,L"$1\rend",false,false,true);
  }
  std::cout<<"PASS: "<<trials<<" differential scenarios, replacement cache transitions and all-replace-equivalence\n";
}
"""
cpp=prefix+"\nnamespace old {\n"+sources(baseline)+"}\nnamespace neo {\n"+sources(candidate)+"}\n"+tail
with tempfile.TemporaryDirectory() as td:
    source=Path(td)/"search_compare.cpp"
    exe=Path(td)/"search_compare"
    source.write_text(cpp,encoding="utf-8")
    subprocess.run(["g++","-std=c++17","-O2","-Wall","-Wextra",str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=100)
