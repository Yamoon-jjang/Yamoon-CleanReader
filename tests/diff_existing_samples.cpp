// Byte-for-byte differential check against main for established sample fixtures.
#include "engine.h"
#include <codecvt>
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
        catch (...) { std::cerr << "Cannot decode " << path << '\n'; return 1; }
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
