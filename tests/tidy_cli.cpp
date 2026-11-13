// 시험용 명령줄 도구: 표준 입력(UTF-8) 글을 기본 옵션(또는 명령줄 옵션)으로 정리해 표준 출력(UTF-8)으로 냄.
// 배포판(exe)에는 들어가지 않음 — tests/run_cases.py가 src/engine.cpp와 함께 빌드해 씀.
#include "../src/engine.h"
#include <cstdio>
#include <iostream>
#include <iterator>
#include <string>

static std::u32string Dec(const std::string& s) {
    std::u32string r;
    size_t i = 0;
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) i = 3;
    while (i < s.size()) {
        unsigned char c = s[i];
        int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        char32_t u = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; k < n && i + k < s.size(); ++k) u = (u << 6) | (s[i + k] & 0x3F);
        r += u;
        i += n;
    }
    return r;
}
static std::string Enc(const std::u32string& s) {
    std::string r;
    for (char32_t u : s) {
        if (u < 0x80) r += char(u);
        else if (u < 0x800) { r += char(0xC0 | (u >> 6)); r += char(0x80 | (u & 63)); }
        else if (u < 0x10000) { r += char(0xE0 | (u >> 12)); r += char(0x80 | ((u >> 6) & 63)); r += char(0x80 | (u & 63)); }
        else { r += char(0xF0 | (u >> 18)); r += char(0x80 | ((u >> 12) & 63)); r += char(0x80 | ((u >> 6) & 63)); r += char(0x80 | (u & 63)); }
    }
    return r;
}
int main(int argc, char** argv) {
    std::string in((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    TidyOptions opt;
    // 사례 폴더의 options.txt에 적은 옵션 (없으면 기본 옵션): --junk = "쪽 번호·광고·HTML 제거" 켬
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--junk") opt.removeJunk = true;
    TidyReport rep;
    std::cout << Enc(TidyProcess(Dec(in), opt, &rep));
    return 0;
}
