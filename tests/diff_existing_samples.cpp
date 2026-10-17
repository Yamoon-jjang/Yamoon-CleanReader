// Byte-for-byte differential check against main for established sample fixtures.
#include "engine.h"
#include <codecvt>
#include <iconv.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <locale>
#include <string>

int main() {
    const char* samples[] = {
        "sample_blank_lines.txt", "sample_chapters.txt", "sample_double_space.txt",
        "sample_korean_special.txt", "sample_messy.txt", "sample_mobile_fixedwidth.txt",
        "sample_one_line.txt", "sample_scene_break.txt", "sample_uneven_gaps.txt"
    };
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> utf8;
    for (auto* name : samples) {
        std::string path = std::string("dist/samples/") + name;
        std::ifstream file(path, std::ios::binary);
        if (!file) { std::cerr << "Cannot read " << path << '\n'; return 1; }
        std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::u32string input;
        try { input = utf8.from_bytes(bytes); }
        catch (...) {
            // Several legacy fixtures deliberately use a Korean Windows code page.
            iconv_t converter = iconv_open("UTF-8", "CP949");
            if (converter == (iconv_t)-1) { std::cerr << "CP949 codec missing\n"; return 1; }
            std::string decoded(bytes.size() * 4 + 16, '\0');
            char* in = bytes.data();
            size_t remainingIn = bytes.size();
            char* out = decoded.data();
            size_t remainingOut = decoded.size();
            size_t status = iconv(converter, &in, &remainingIn, &out, &remainingOut);
            iconv_close(converter);
            if (status == (size_t)-1 || remainingIn != 0) {
                std::cerr << "Cannot decode as UTF-8 or CP949: " << path << '\n';
                return 1;
            }
            decoded.resize(decoded.size() - remainingOut);
            input = utf8.from_bytes(decoded);
        }
        for (int mode = 0; mode < 7; ++mode) {
            TidyOptions options;
            switch (mode) {
            case 1: options.joinMode = 1; break;
            case 2: options.blankMode = 1; break;
            case 3: options.blankMode = 2; break;
            case 4: options.splitMode = 2; break;
            case 5: options.removeJunk = true; options.stripHanja = true; break;
            case 6: options.spaceMode = 1; options.quoteMode = 1; break;
            }
            std::u32string output = TidyProcess(input, options, nullptr, nullptr);
            std::string encoded = utf8.to_bytes(output);
            std::cout << path << ":" << mode << ":" << encoded.size() << "\n";
            std::cout.write(encoded.data(), encoded.size());
            std::cout << "\n";
        }
    }
    return 0;
}
