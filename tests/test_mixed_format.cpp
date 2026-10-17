// Regression tests for mixed-format novels. No Win32 dependencies.
#include "../src/engine.h"
#include <cstdlib>
#include <iostream>
#include <string>

using S = std::u32string;

static void require(bool ok, const char* name) {
    if (!ok) { std::cerr << "FAIL " << name << '\n'; std::exit(1); }
    std::cout << "PASS " << name << '\n';
}
static bool has(const S& text, const S& phrase) {
    return text.find(phrase) != S::npos;
}
static S tidy(const S& text, TidyOptions options = {}) {
    options.splitMode = 2;  // Isolate paragraph restoration from sentence splitting.
    options.sceneMode = 2;
    return TidyProcess(text, options, nullptr, nullptr);
}

static const S broken =
    U"내 몸과의 생소한 만남\n\n"
    U"불빛 한 점 없는 어두운 방,\n\n"
    U"나는 오늘도 가려운 몸을\n\n"
    U"벅벅 긁고 일어나 컴퓨터를 켠다.\n\n"
    U"내 딸을 만나기 위해서.\n\n"
    U"[유시~!! 쿄모 간바로!!]\n\n"
    U"\"그래, 힘낼게..\"\n\n"
    U"게임을 키자마자 들려오는\n\n"
    U"익숙한 로그인 사운드.\n\n"
    U"이제 하루에 한 번씩 듣지 않는다면 온몸에 가시가 돋을 지경이었다.\n\n"
    U"지금 내가 하는 게임의\n\n"
    U"제목은 바로 유시무스메,\n\n"
    U"한국어로 번역하면 용사딸 정도로 해석할 수 있는 게임이었다.\n\n"
    U"게임 제목에 딸과 용사가\n\n"
    U"들어간다는 점에서 유추할 수 있듯이\n\n"
    U"이 게임은 괴수들이 나타나는\n\n"
    U"현대판타지 세계관에서 딸을\n\n"
    U"가장한 용사를 키우는 게임이다.";

static void expectRecovered(const S& text, const char* prefix) {
    require(has(text, U"나는 오늘도 가려운 몸을 벅벅 긁고 일어나 컴퓨터를 켠다."), prefix);
    require(has(text, U"게임을 키자마자 들려오는 익숙한 로그인 사운드."), "login sound joined");
    require(has(text, U"지금 내가 하는 게임의 제목은 바로 유시무스메,"), "game title joined");
    require(has(text, U"게임 제목에 딸과 용사가 들어간다는 점에서 유추할 수 있듯이"), "game description joined");
    require(has(text, U"내 딸을 만나기 위해서.\n\n[유시"), "completed sentence and bracket dialogue stay separate");
}

int main() {
    S small = tidy(broken);
    expectRecovered(small, "paste-size source joins");

    // A normal novel can have tens of thousands of already complete paragraphs.
    // Their global statistics must not disable repairs in a small mixed-in section.
    const S normal = U"오늘의 이야기는 여기에서 마무리되었다. 내일 새로운 이야기가 시작된다.\n";
    S tail;
    tail.reserve(5'500'000);
    for (int i = 0; i < 60000; ++i) tail += normal;
    require(tail.size() >= 2'000'000, "large normal body prepared (over 5 MB UTF-8)");

    S front = tidy(broken + U"\n\n" + tail);
    expectRecovered(front, "broken section at start of large novel");
    require(has(front, U"오늘의 이야기는 여기에서 마무리되었다. 내일 새로운 이야기가 시작된다."),
            "normal body stays legible");

    S middle = tidy(tail + U"\n\n" + broken + U"\n\n" + tail);
    expectRecovered(middle, "broken section in middle of large novel");

    S end = tidy(tail + U"\n\n" + broken);
    expectRecovered(end, "broken section at end of large novel");

    // Already well-formed paragraphs and explicit user options must not regress.
    const S clean = U"첫 번째 문단은 여기에서 끝난다.\n\n두 번째 문단은 여기서 시작하고 끝난다.\n\n세 번째 문단도 분리한다.";
    require(has(tidy(clean), U"첫 번째 문단은 여기에서 끝난다.\n\n두 번째 문단"),
            "genuine paragraph gap survives");
    const S verse = U"별 하나\n\n별 둘\n\n별 셋\n\n밤하늘\n\n바람 소리";
    require(!has(tidy(verse), U"별 하나 별 둘"), "short intentionally separated verse remains");
    TidyOptions noJoin;
    noJoin.joinMode = 1;
    require(!has(tidy(broken, noJoin), U"나는 오늘도 가려운 몸을 벅벅"), "explicit never-join option honored");
    TidyOptions hardBlanks;
    hardBlanks.blankMode = 1;
    require(!has(tidy(broken + U"\n\n" + tail, hardBlanks),
                 U"나는 오늘도 가려운 몸을 벅벅"), "explicit paragraph-gap option honored");

    std::cout << "PASS all mixed-format regressions\n";
}
