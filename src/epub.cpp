// 야문 클린리더 - EPUB 글 꺼내기 (플랫폼 독립)
// EPUB = ZIP 압축 파일 안의 XHTML 문서들. 압축 풀기(deflate)·ZIP 목록·차례(OPF spine)·본문 글 꺼내기를 직접 처리함
// (추가 DLL·외부 라이브러리 없음)
#include "epub.h"
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <type_traits>

namespace {

typedef std::u32string U32;

// ---------------------------------------------------------------------------
// deflate 풀기 (RFC 1951)
struct BitIn {
    const unsigned char* in;
    size_t len, pos = 0;
    uint32_t buf = 0;
    int cnt = 0;
    bool err = false;
    int bits(int n) {
        while (cnt < n) {
            if (pos >= len) { err = true; return 0; }
            buf |= (uint32_t)in[pos++] << cnt;
            cnt += 8;
        }
        int v = (int)(buf & ((1u << n) - 1));
        buf >>= n;
        cnt -= n;
        return v;
    }
};
struct Huff {
    short count[16];
    short symbol[288];
};
// 길이 표로 허프만 표를 만듦. 길이가 넘치면(잘못된 표) false
bool BuildHuff(Huff& h, const short* length, int n) {
    for (int i = 0; i < 16; ++i) h.count[i] = 0;
    for (int s = 0; s < n; ++s) h.count[length[s]]++;
    if (h.count[0] == n) return true;  // 코드 없음 (거리 표에서 가능)
    int left = 1;
    for (int len = 1; len < 16; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return false;
    }
    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; ++len) offs[len + 1] = (short)(offs[len] + h.count[len]);
    for (int s = 0; s < n; ++s)
        if (length[s] != 0) h.symbol[offs[length[s]]++] = (short)s;
    return true;
}
int Decode(BitIn& b, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= b.bits(1);
        if (b.err) return -1;
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;  // 없는 코드
}
const short kLBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const short kLExt[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const short kDBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const short kDExt[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool Codes(BitIn& b, std::string& out, const Huff& lc, const Huff& dc, size_t limit) {
    for (;;) {
        int sym = Decode(b, lc);
        if (sym < 0) return false;
        if (sym < 256) {
            if (out.size() >= limit) return false;
            out += (char)sym;
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            int len = kLBase[sym] + b.bits(kLExt[sym]);
            int ds = Decode(b, dc);
            if (ds < 0 || ds >= 30) return false;
            size_t dist = (size_t)(kDBase[ds] + b.bits(kDExt[ds]));
            if (b.err || dist > out.size() || out.size() + (size_t)len > limit) return false;
            size_t from = out.size() - dist;
            for (int k = 0; k < len; ++k) out += out[from + (size_t)k];  // 겹쳐 복사될 수 있어 한 글자씩
        }
    }
}

}  // namespace

bool InflateRaw(const unsigned char* in, size_t inLen, std::string& out, size_t expect) {
    out.clear();
    const size_t limit = 512u * 1024 * 1024;  // 이상하게 큰 압축(압축 폭탄) 막기
    if (expect && expect < limit) out.reserve(expect);
    BitIn b{in, inLen};
    int last;
    do {
        last = b.bits(1);
        int type = b.bits(2);
        if (b.err) return false;
        if (type == 0) {  // 압축 안 한 블록
            b.buf = 0; b.cnt = 0;
            if (b.pos + 4 > b.len) return false;
            unsigned n = b.in[b.pos] | (b.in[b.pos + 1] << 8);
            unsigned nc = b.in[b.pos + 2] | (b.in[b.pos + 3] << 8);
            b.pos += 4;
            if (n != (~nc & 0xFFFF) || b.pos + n > b.len || out.size() + n > limit) return false;
            out.append((const char*)b.in + b.pos, n);
            b.pos += n;
        } else if (type == 1) {  // 고정 허프만 (표는 한 번만 만듦: 여러 파일 변환의 작업 스레드에서도 안전)
            struct Fixed { Huff lc, dc; };
            static const Fixed fx = [] {
                Fixed f;
                short l[288];
                int s = 0;
                for (; s < 144; ++s) l[s] = 8;
                for (; s < 256; ++s) l[s] = 9;
                for (; s < 280; ++s) l[s] = 7;
                for (; s < 288; ++s) l[s] = 8;
                BuildHuff(f.lc, l, 288);
                for (s = 0; s < 30; ++s) l[s] = 5;
                BuildHuff(f.dc, l, 30);
                return f;
            }();
            if (!Codes(b, out, fx.lc, fx.dc, limit)) return false;
        } else if (type == 2) {  // 동적 허프만
            int nlen = b.bits(5) + 257, ndist = b.bits(5) + 1, ncode = b.bits(4) + 4;
            if (b.err || nlen > 286 || ndist > 30) return false;
            static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            short lengths[320];
            for (int i = 0; i < 19; ++i) lengths[order[i]] = i < ncode ? (short)b.bits(3) : 0;
            Huff cl;
            if (b.err || !BuildHuff(cl, lengths, 19)) return false;
            int idx = 0;
            while (idx < nlen + ndist) {
                int sym = Decode(b, cl);
                if (sym < 0) return false;
                if (sym < 16) { lengths[idx++] = (short)sym; continue; }
                short v = 0;
                int rep;
                if (sym == 16) {
                    if (idx == 0) return false;
                    v = lengths[idx - 1];
                    rep = 3 + b.bits(2);
                } else if (sym == 17) rep = 3 + b.bits(3);
                else rep = 11 + b.bits(7);
                if (b.err || idx + rep > nlen + ndist) return false;
                while (rep--) lengths[idx++] = v;
            }
            if (lengths[256] == 0) return false;
            Huff lc, dc;
            if (!BuildHuff(lc, lengths, nlen) || !BuildHuff(dc, lengths + nlen, ndist)) return false;
            if (!Codes(b, out, lc, dc, limit)) return false;
        } else {
            return false;
        }
    } while (!last);
    return true;
}

namespace {

// ---------------------------------------------------------------------------
// ZIP 목록
struct ZEntry { std::string name; int method = 0; uint32_t csize = 0, usize = 0, off = 0; bool enc = false; };
uint32_t U16(const std::string& s, size_t p) { return (unsigned char)s[p] | ((unsigned char)s[p + 1] << 8); }
uint32_t U32le(const std::string& s, size_t p) { return U16(s, p) | (U16(s, p + 2) << 16); }

bool ReadZipDir(const std::string& z, std::vector<ZEntry>& es) {
    if (z.size() < 22) return false;
    size_t lo = z.size() > 65557 ? z.size() - 65557 : 0, e = std::string::npos;
    for (size_t p = z.size() - 22 + 1; p-- > lo;)
        if (U32le(z, p) == 0x06054b50) { e = p; break; }
    if (e == std::string::npos) return false;
    size_t n = U16(z, e + 10), cdOff = U32le(z, e + 16);
    if (cdOff == 0xFFFFFFFFu || cdOff >= z.size()) return false;  // ZIP64(4GB 넘는 압축)는 쓰지 않음
    size_t p = cdOff;
    for (size_t i = 0; i < n; ++i) {
        if (p + 46 > z.size() || U32le(z, p) != 0x02014b50) return false;
        ZEntry en;
        en.enc = (U16(z, p + 8) & 1) != 0;
        en.method = (int)U16(z, p + 10);
        en.csize = U32le(z, p + 20);
        en.usize = U32le(z, p + 24);
        size_t nl = U16(z, p + 28), el = U16(z, p + 30), cl = U16(z, p + 32);
        en.off = U32le(z, p + 42);
        if (p + 46 + nl > z.size()) return false;
        en.name = z.substr(p + 46, nl);
        es.push_back(en);
        p += 46 + nl + el + cl;
    }
    return true;
}
bool ReadEntry(const std::string& z, const ZEntry& en, std::string& out) {
    size_t p = en.off;
    if (en.enc || p + 30 > z.size() || U32le(z, p) != 0x04034b50) return false;
    size_t d = p + 30 + U16(z, p + 26) + U16(z, p + 28);
    if (d > z.size() || en.csize > z.size() - d) return false;
    if (en.method == 0) { out.assign(z, d, en.csize); return true; }
    if (en.method == 8) return InflateRaw((const unsigned char*)z.data() + d, en.csize, out, en.usize);
    return false;
}

// ---------------------------------------------------------------------------
// 글자 바꾸기
void PutU8(std::string& o, char32_t c) {
    if (c < 0x80) o += (char)c;
    else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 63)); }
    else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
    else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 63)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
}
U32 FromUtf8(const unsigned char* p, size_t n) {
    U32 r;
    r.reserve(n);
    size_t i = 0;
    while (i < n) {
        unsigned c = p[i];
        int len = c < 0x80 ? 1 : (c >= 0xC2 && c <= 0xDF) ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c >= 0xF0 && c <= 0xF4) ? 4 : 0;
        if (len == 0 || i + len > n) { r += 0xFFFD; ++i; continue; }
        char32_t cp = len == 1 ? c : len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) : (c & 0x07);
        bool ok = true;
        for (int k = 1; k < len; ++k) {
            unsigned cc = p[i + k];
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok || (len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) { r += 0xFFFD; ++i; continue; }
        r += cp;
        i += len;
    }
    return r;
}
U32 FromUtf16(const unsigned char* p, size_t n, bool be) {
    U32 r;
    r.reserve(n / 2);
    for (size_t i = 0; i + 1 < n; i += 2) {
        unsigned u = be ? (p[i] << 8 | p[i + 1]) : (p[i] | p[i + 1] << 8);
        if (u >= 0xD800 && u <= 0xDBFF && i + 3 < n) {
            unsigned v = be ? (p[i + 2] << 8 | p[i + 3]) : (p[i + 2] | p[i + 3] << 8);
            if (v >= 0xDC00 && v <= 0xDFFF) { r += (char32_t)(0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00)); i += 2; continue; }
        }
        r += (char32_t)u;
    }
    return r;
}
std::string Lower(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32); return s; }

// 문서(XML·XHTML) 바이트 → 글자. BOM과 xml 선언의 encoding을 따름
U32 DecodeDoc(const std::string& b, EpubDecodeFn other) {
    const unsigned char* u = (const unsigned char*)b.data();
    size_t n = b.size();
    if (n >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) return FromUtf8(u + 3, n - 3);
    if (n >= 2 && u[0] == 0xFF && u[1] == 0xFE) return FromUtf16(u + 2, n - 2, false);
    if (n >= 2 && u[0] == 0xFE && u[1] == 0xFF) return FromUtf16(u + 2, n - 2, true);
    if (n >= 4 && u[0] == '<' && u[1] == 0 && u[2] == '?' && u[3] == 0) return FromUtf16(u, n, false);
    if (n >= 4 && u[0] == 0 && u[1] == '<' && u[2] == 0 && u[3] == '?') return FromUtf16(u, n, true);
    std::string head = Lower(b.substr(0, std::min<size_t>(n, 200)));
    size_t e = head.find("encoding");
    if (head.compare(0, 5, "<?xml") == 0 && e != std::string::npos && other) {
        size_t q = head.find_first_of("'\"", e);
        if (q != std::string::npos) {
            size_t q2 = head.find(head[q], q + 1);
            std::string enc = q2 == std::string::npos ? "" : head.substr(q + 1, q2 - q - 1);
            if (!enc.empty() && enc != "utf-8" && enc != "utf8" && enc != "us-ascii" && enc != "ascii") return other(b);
        }
    }
    return FromUtf8(u, n);
}

// ---------------------------------------------------------------------------
// 아주 단순한 태그 읽기 (XML·XHTML 공통)
struct Tag {
    std::string name;  // 소문자, 접두어(opf: 등) 뺀 이름
    bool close = false, selfClose = false;
    std::vector<std::pair<std::string, std::string>> attrs;  // 이름은 소문자·접두어 뺌
    const std::string* Attr(const char* k) const {
        for (auto& a : attrs) if (a.first == k) return &a.second;
        return nullptr;
    }
};
// 속성 값 모으기: 바이트 글(UTF-8)은 그대로, 글자 글(char32_t)은 UTF-8로
inline void AddCh(std::string& o, char c) { o += c; }
inline void AddCh(std::string& o, char32_t c) { PutU8(o, c); }
inline char32_t UC(char c) { return (unsigned char)c; }
inline char32_t UC(char32_t c) { return c; }
// s[i] == '<' 에서 태그 하나를 읽음. 끝(>) 다음 위치를 돌려줌. 주석·선언이면 tag.name이 빔
template <class Str>
size_t ReadTag(const Str& s, size_t i, Tag& t) {
    t = Tag();
    size_t n = s.size();
    auto starts = [&](size_t p, const char* w) {
        for (size_t k = 0; w[k]; ++k) if (p + k >= n || UC(s[p + k]) != (char32_t)(unsigned char)w[k]) return false;
        return true;
    };
    if (starts(i, "<!--")) {
        for (size_t p = i + 4; p + 2 < n; ++p) if (s[p] == '-' && s[p + 1] == '-' && s[p + 2] == '>') return p + 3;
        return n;
    }
    if (starts(i, "<![CDATA[")) {  // 글 그대로: 부르는 쪽이 처리하도록 이름만 표시
        t.name = "#cdata";
        return i + 9;
    }
    if (i + 1 < n && (s[i + 1] == '!' || s[i + 1] == '?')) {
        for (size_t p = i + 1; p < n; ++p) if (s[p] == '>') return p + 1;
        return n;
    }
    size_t p = i + 1;
    if (p < n && s[p] == '/') { t.close = true; ++p; }
    auto isNameCh = [](char32_t c) { return c > ' ' && c != '>' && c != '/' && c != '=' && c != '"' && c != '\''; };
    auto lowerAscii = [](char32_t c) -> char { return (char)((c >= 'A' && c <= 'Z') ? c + 32 : (c < 0x80 ? c : '_')); };
    auto local = [](std::string x) { size_t c = x.find(':'); return c == std::string::npos ? x : x.substr(c + 1); };
    std::string name;
    while (p < n && isNameCh(UC(s[p]))) name += lowerAscii(UC(s[p++]));
    if (name.empty()) return i + 1;  // "<" 다음이 이름이 아니면 글자로 봄 (부르는 쪽에서 처리)
    t.name = local(name);
    for (;;) {
        while (p < n && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) ++p;
        if (p >= n) return n;
        if (s[p] == '>') return p + 1;
        if (s[p] == '/') { t.selfClose = true; ++p; continue; }
        std::string an;
        while (p < n && isNameCh(UC(s[p]))) an += lowerAscii(UC(s[p++]));
        if (an.empty()) { ++p; continue; }
        while (p < n && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) ++p;
        std::string av;
        if (p < n && s[p] == '=') {
            ++p;
            while (p < n && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) ++p;
            if (p < n && (s[p] == '"' || s[p] == '\'')) {
                char32_t q = UC(s[p++]);
                std::string raw;
                while (p < n && UC(s[p]) != q) { AddCh(raw, s[p]); ++p; }
                if (p < n) ++p;
                av = raw;
            } else {
                while (p < n && UC(s[p]) > ' ' && s[p] != '>') { AddCh(av, s[p]); ++p; }
            }
        }
        t.attrs.push_back({local(an), av});
    }
}

// 이름 있는 문자 (자주 쓰이는 것)
char32_t NamedEntity(const std::string& nm) {
    static const std::map<std::string, char32_t> m = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''}, {"nbsp", 0xA0},
        {"hellip", 0x2026}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"lsquo", 0x2018}, {"rsquo", 0x2019},
        {"ldquo", 0x201C}, {"rdquo", 0x201D}, {"sbquo", 0x201A}, {"bdquo", 0x201E}, {"middot", 0xB7}, {"bull", 0x2022},
        {"copy", 0xA9}, {"reg", 0xAE}, {"trade", 0x2122}, {"deg", 0xB0}, {"times", 0xD7}, {"divide", 0xF7},
        {"laquo", 0xAB}, {"raquo", 0xBB}, {"lsaquo", 0x2039}, {"rsaquo", 0x203A}, {"thinsp", 0x2009}, {"ensp", 0x2002},
        {"emsp", 0x2003}, {"zwsp", 0x200B}, {"zwnj", 0x200C}, {"zwj", 0x200D}, {"shy", 0xAD}, {"prime", 0x2032},
        {"Prime", 0x2033}, {"sect", 0xA7}, {"para", 0xB6}, {"dagger", 0x2020}, {"Dagger", 0x2021}, {"larr", 0x2190},
        {"rarr", 0x2192}, {"uarr", 0x2191}, {"darr", 0x2193}, {"hearts", 0x2665}, {"star", 0x2606}, {"iexcl", 0xA1},
        {"iquest", 0xBF}, {"cent", 0xA2}, {"pound", 0xA3}, {"yen", 0xA5}, {"euro", 0x20AC}, {"frac12", 0xBD},
        {"frac14", 0xBC}, {"frac34", 0xBE}, {"plusmn", 0xB1}, {"micro", 0xB5}, {"oline", 0x203E}, {"loz", 0x25CA},
    };
    auto it = m.find(nm);
    return it == m.end() ? 0 : it->second;
}

bool IsAsciiWS(char32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f'; }

// XHTML 본문 → 문단들
struct TextOut {
    std::vector<U32> paras;  // 빈 문단(내용 없는 칸)은 빈 글로 남겨 둠: 장면 구분 판단용
    U32 cur;
    bool space = false;      // 다음 글자 앞에 공백 하나
    void Flush() {
        // 줄(<br>)마다 앞뒤 공백을 다듬음 (보이지 않는 서식 문자는 그대로)
        U32 r;
        size_t a = 0;
        bool any = false;
        while (a <= cur.size()) {
            size_t b = cur.find(U'\n', a);
            if (b == U32::npos) b = cur.size();
            size_t s = a, e = b;
            while (s < e && IsAsciiWS(cur[s])) ++s;
            while (e > s && IsAsciiWS(cur[e - 1])) --e;
            if (any) r += U'\n';
            r.append(cur, s, e - s);
            any = true;
            a = b + 1;
        }
        while (!r.empty() && r.back() == U'\n') r.pop_back();
        while (!r.empty() && r.front() == U'\n') r.erase(r.begin());
        if (!r.empty() || !cur.empty()) paras.push_back(r);
        cur.clear();
        space = false;
    }
    void Char(char32_t c) {
        if (space && !cur.empty() && cur.back() != U'\n') cur += U' ';
        space = false;
        cur += c;
    }
};

bool IsBlock(const std::string& n) {
    static const char* b[] = {"p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "li", "ul", "ol", "dl", "dt", "dd", "blockquote",
                              "section", "article", "header", "footer", "aside", "nav", "figure", "figcaption", "table", "tr",
                              "pre", "body", "main", "center", "address", "caption", "tbody", "thead", "tfoot", "hgroup", "details", "summary", nullptr};
    for (int i = 0; b[i]; ++i) if (n == b[i]) return true;
    return false;
}
bool IsSkip(const std::string& n) {  // 안의 글을 보이지 않는 요소
    return n == "head" || n == "script" || n == "style" || n == "rt" || n == "rp" || n == "svg" || n == "math" || n == "template" || n == "noscript";
}

void XhtmlText(const U32& s, TextOut& o) {
    size_t n = s.size(), i = 0;
    int skip = 0, pre = 0;
    std::vector<std::string> skipStack;
    while (i < n) {
        char32_t c = s[i];
        if (c == '<') {
            Tag t;
            size_t e = ReadTag(s, i, t);
            if (t.name.empty() && e == i + 1) {  // 이름 없는 "<": 글자 그대로
                if (!skip) o.Char(c);
                i = e;
                continue;
            }
            if (t.name == "#cdata") {  // CDATA: 글 그대로
                size_t q = s.find(U"]]>", e);
                if (q == U32::npos) q = n;
                if (!skip) for (size_t k = e; k < q; ++k) o.Char(s[k]);
                i = q == n ? n : q + 3;
                continue;
            }
            i = e;
            if (t.name.empty()) continue;
            if (IsSkip(t.name)) {
                if (t.selfClose) continue;
                if (!t.close) { skip++; skipStack.push_back(t.name); }
                else if (skip) {
                    // 짝이 맞는 것까지 닫음 (잘못 짝지어진 문서 대비)
                    auto it = std::find(skipStack.rbegin(), skipStack.rend(), t.name);
                    if (it != skipStack.rend()) {
                        size_t keep = skipStack.size() - 1 - (size_t)(it - skipStack.rbegin());
                        skipStack.resize(keep);
                        skip = (int)keep;
                    }
                }
                continue;
            }
            if (skip) continue;
            if (t.name == "br") { o.cur += U'\n'; o.space = false; continue; }
            if (t.name == "hr") { o.Flush(); o.paras.push_back(U32()); continue; }  // 가로줄: 빈 칸 하나 (장면 구분)
            if (t.name == "img" || t.name == "image") continue;
            if (t.name == "pre") { o.Flush(); pre += t.close ? -1 : (t.selfClose ? 0 : 1); if (pre < 0) pre = 0; continue; }
            if (t.name == "td" || t.name == "th") { o.space = true; continue; }
            if (IsBlock(t.name)) o.Flush();
            continue;
        }
        if (skip) { ++i; continue; }
        if (c == '&') {  // 문자 참조
            size_t semi = i + 1;
            while (semi < n && semi - i < 12 && s[semi] != ';' && s[semi] != '<' && s[semi] != '&' && !IsAsciiWS(s[semi])) ++semi;
            char32_t v = 0;
            if (semi < n && s[semi] == ';') {
                if (semi > i + 2 && s[i + 1] == '#') {
                    bool hex = s[i + 2] == 'x' || s[i + 2] == 'X';
                    unsigned long long x = 0;
                    bool ok = semi > i + (hex ? 3 : 2);
                    for (size_t k = i + (hex ? 3 : 2); k < semi && ok; ++k) {
                        char32_t d = s[k];
                        int dv = (d >= '0' && d <= '9') ? (int)(d - '0') : (hex && d >= 'a' && d <= 'f') ? (int)(d - 'a' + 10) : (hex && d >= 'A' && d <= 'F') ? (int)(d - 'A' + 10) : -1;
                        if (dv < 0) ok = false;
                        else x = x * (hex ? 16 : 10) + (unsigned)dv;
                        if (x > 0x10FFFF) ok = false;
                    }
                    if (ok && x > 0 && !(x >= 0xD800 && x <= 0xDFFF)) v = (char32_t)x;
                } else {
                    std::string nm;
                    for (size_t k = i + 1; k < semi; ++k) nm += (char)(s[k] < 0x80 ? s[k] : '_');
                    v = NamedEntity(nm);
                }
            }
            if (v) {
                if (pre && v == '\n') o.cur += U'\n';
                else o.Char(v);
                i = semi + 1;
                continue;
            }
            o.Char(c);  // 알 수 없으면 글자 그대로
            ++i;
            continue;
        }
        if (IsAsciiWS(c)) {
            if (pre) {
                if (c == '\n') o.cur += U'\n';
                else if (c != '\r') o.cur += c;
            } else {
                o.space = true;
            }
            ++i;
            continue;
        }
        o.Char(c);
        ++i;
    }
    o.Flush();
}

// 경로 다듬기: "a/b/../c" → "a/c", %xx 풀기, #조각 빼기
std::string UrlDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            r += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else r += s[i];
    }
    return r;
}
std::string JoinPath(const std::string& dir, std::string href) {
    size_t h = href.find('#');
    if (h != std::string::npos) href.resize(h);
    href = UrlDecode(href);
    std::string full = (!href.empty() && href[0] == '/') ? href.substr(1) : dir + href;
    std::vector<std::string> parts;
    size_t a = 0;
    while (a <= full.size()) {
        size_t b = full.find('/', a);
        if (b == std::string::npos) b = full.size();
        std::string seg = full.substr(a, b - a);
        if (seg == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (!seg.empty() && seg != ".") parts.push_back(seg);
        a = b + 1;
    }
    std::string r;
    for (size_t k = 0; k < parts.size(); ++k) { if (k) r += '/'; r += parts[k]; }
    return r;
}

}  // namespace

bool EpubExtract(const std::string& z, std::u32string& out, std::wstring& why, EpubDecodeFn decodeOther) {
    out.clear();
    std::vector<ZEntry> es;
    if (!ReadZipDir(z, es)) { why = L"손상되었거나 EPUB이 아닌 파일"; return false; }
    std::map<std::string, size_t> byName, byLower;
    for (size_t k = 0; k < es.size(); ++k) { byName[es[k].name] = k; byLower[Lower(es[k].name)] = k; }
    auto find = [&](const std::string& nm) -> const ZEntry* {
        auto it = byName.find(nm);
        if (it != byName.end()) return &es[it->second];
        auto il = byLower.find(Lower(nm));
        return il == byLower.end() ? nullptr : &es[il->second];
    };
    auto read = [&](const std::string& nm, std::string& data) {
        const ZEntry* e = find(nm);
        return e && ReadEntry(z, *e, data);
    };
    auto toUtf8 = [&](const std::string& b) {  // OPF·container는 태그만 읽으므로 UTF-8로 맞춤
        U32 t = DecodeDoc(b, decodeOther);
        std::string r;
        for (char32_t c : t) PutU8(r, c);
        return r;
    };
    // 1) 차례 파일(OPF) 위치
    std::string data, opfPath;
    if (read("META-INF/container.xml", data)) {
        std::string x = toUtf8(data);
        for (size_t i = x.find('<'); i != std::string::npos && i < x.size(); i = x.find('<', i)) {
            Tag t;
            i = ReadTag(x, i, t);
            if (t.name == "rootfile" && !t.close) {
                const std::string* fp = t.Attr("full-path");
                const std::string* mt = t.Attr("media-type");
                if (fp && (!mt || *mt == "application/oebps-package+xml")) { opfPath = *fp; break; }
            }
        }
    }
    if (opfPath.empty())  // container.xml이 없거나 잘못됨: .opf 파일을 찾아 씀
        for (auto& e : es) if (Lower(e.name).size() > 4 && Lower(e.name).compare(Lower(e.name).size() - 4, 4, ".opf") == 0) { opfPath = e.name; break; }
    if (opfPath.empty() || !read(opfPath, data)) { why = L"손상되었거나 EPUB이 아닌 파일 (차례 정보 없음)"; return false; }
    std::string opfDir = opfPath.find('/') == std::string::npos ? "" : opfPath.substr(0, opfPath.rfind('/') + 1);
    // 2) 목록(manifest)과 읽는 순서(spine)
    std::string x = toUtf8(data);
    std::map<std::string, std::pair<std::string, std::string>> items;  // id -> (경로, 형식)
    std::vector<std::string> spine;
    for (size_t i = x.find('<'); i != std::string::npos && i < x.size(); i = x.find('<', i)) {
        Tag t;
        i = ReadTag(x, i, t);
        if (t.close) continue;
        if (t.name == "item") {
            const std::string *id = t.Attr("id"), *href = t.Attr("href"), *mt = t.Attr("media-type");
            if (id && href) items[*id] = {JoinPath(opfDir, *href), mt ? Lower(*mt) : std::string()};
        } else if (t.name == "itemref") {
            const std::string* idref = t.Attr("idref");
            if (idref) spine.push_back(*idref);
        }
    }
    std::vector<std::string> docs;
    for (auto& id : spine) {
        auto it = items.find(id);
        if (it == items.end()) continue;
        const std::string& mt = it->second.second;
        if (!mt.empty() && mt.find("html") == std::string::npos && mt.find("xml") == std::string::npos) continue;  // 그림 등
        docs.push_back(it->second.first);
    }
    if (docs.empty()) { why = L"본문이 없는 EPUB"; return false; }
    // 3) 본문이 암호화(DRM)되어 있는지: encryption.xml에 본문 문서가 있으면 읽을 수 없음 (글꼴만 가린 것은 괜찮음)
    if (read("META-INF/encryption.xml", data)) {
        std::string ex = toUtf8(data);
        for (size_t i = ex.find('<'); i != std::string::npos && i < ex.size(); i = ex.find('<', i)) {
            Tag t;
            i = ReadTag(ex, i, t);
            if (t.name != "cipherreference" || t.close) continue;
            const std::string* uri = t.Attr("uri");
            if (!uri) continue;
            std::string p = JoinPath("", *uri);
            for (auto& d : docs) if (Lower(d) == Lower(p)) { why = L"DRM(복제 방지)으로 보호된 EPUB"; return false; }
        }
    }
    // 4) 문서마다 본문 글 꺼내기
    TextOut o;
    size_t got = 0;
    for (auto& d : docs) {
        std::string b;
        if (!read(d, b)) continue;
        ++got;
        o.Flush();
        U32 doc = DecodeDoc(b, decodeOther);
        XhtmlText(doc, o);
    }
    if (got == 0) { why = L"본문을 읽을 수 없는 EPUB"; return false; }
    // 5) 문단은 빈 줄로 나눔. 내용 없는 칸(<p>&nbsp;</p>, <hr>)이 있던 곳은 빈 줄 하나를 더 둠 (장면 전환 판단은 정리 엔진이 함)
    bool started = false;
    int gap = 0;  // 0 문단 사이, 1 넓은 사이
    for (auto& p : o.paras) {
        bool blank = true;
        for (char32_t c : p) if (!(IsAsciiWS(c) || c == 0xA0 || c == 0x3000)) { blank = false; break; }
        if (blank) { if (started) gap = 1; continue; }
        if (started) out += gap ? U"\n\n\n" : U"\n\n";
        out += p;
        started = true;
        gap = 0;
    }
    if (!started) { why = L"글이 없는 EPUB (그림으로만 된 책일 수 있음)"; return false; }
    out += U'\n';
    return true;
}
