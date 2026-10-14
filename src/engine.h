// TextTidy - 문단 정리 엔진 (플랫폼 독립)
#pragma once
#include <string>
#include <vector>

struct TidyOptions {
    int  blankMode   = 0;    // 0 자동, 1 빈 줄 = 문단 구분, 2 빈 줄 1개는 무시
    int  joinMode    = 0;    // 0 자동(끊긴 줄 합치기), 1 합치지 않음
    int  spaceMode   = 0;    // 합칠 때 띄어쓰기: 0 자동, 1 항상 띄움, 2 항상 붙임
    int  splitMode   = 1;    // 0 긴 문단만 나눔, 1 문장마다 줄바꿈(기본), 2 나누지 않음
    int  longLen     = 400;  // 긴 문단 기준(글자 수). 넘으면 약 60% 길이 조각으로 나눔
    bool dialogueSep = false; // (사용 안 함) 대사 분리
    int  paraSep     = 0;    // 0 문단 사이 빈 줄, 1 줄바꿈만
    int  sceneMode   = 0;    // 장면 전환(원문에서 드물게 더 넓힌 문단 간격): 0 빈 줄 하나 더, 1 "* * *" 넣기, 2 구분 안 함
    int  indent      = 0;    // 0 없음, 1 공백 1칸, 2 공백 2칸, 3 전각 공백
    bool collapseSp  = true; // 공백·문장부호 다듬기 (연속 공백, 문장부호 앞 공백, 마침표 뒤 공백 누락 등)
    bool removeJunk  = false; // 쪽 번호 줄, HTML 찌꺼기(&nbsp; <br>), 주소만 있는 줄, 반복 머리글 줄 제거
    // 특수문자
    int  quoteMode   = 0;    // 따옴표: 0 그대로, 1 둥근 “ ” ‘ ’, 2 곧은 " '
    int  ellipsisMode = 0;   // 말줄임표: 0 그대로, 1 …, 2 ……, 3 ...
    bool stripHanja  = false; // 괄호 속 한자 지우기: 학교(學校) -> 학교, 學校(학교) -> 학교
    bool fullToHalf  = false; // 전각 영문·숫자·기호 -> 반각 (ＡＢＣ１２３！ -> ABC123!)
    // 사용자 치환: 결과 문단에 차례로 적용. to가 비면 삭제
    std::vector<std::pair<std::u32string, std::u32string>> replaces;
    // 이 글이 들어 있는 원본 줄은 통째로 지움 (광고 문구 등)
    std::vector<std::u32string> deleteLines;
    // 정규식 바꾸기 (ECMAScript 문법, 바꿀 글에 $1 $2 $& 사용 가능). 문단(또는 문장 조각) 안에서 적용
    std::vector<std::pair<std::wstring, std::wstring>> regexReplaces;
    // 이 정규식에 맞는 원본 줄은 통째로 지움
    std::vector<std::wstring> regexDeleteLines;
};

struct TidyReport {
    int  inLines      = 0;   // 원본 줄 수(빈 줄 포함)
    int  textLines    = 0;   // 내용 있는 줄 수
    int  outParas     = 0;   // 결과 문단 수
    bool doubleSpaced = false; // 줄마다 빈 줄이 끼어 있음
    int  wrapWidth    = 0;   // 고정폭 줄바꿈 폭(반각 기준), 0 = 없음
    bool wrapCut      = false; // 글자 단위로 잘린 고정폭(단어 중간 끊김)
    bool indentMarks  = false; // 들여쓰기가 문단 시작을 표시
    bool joined       = false; // 끊긴 줄을 합침
    int  longestLine  = 0;   // 원본 최장 줄(글자)
    int  spaceFixes   = 0;   // 고친 공백·문장부호 수
    int  pageNumLines = 0;   // 지운 쪽 번호 줄 수
    int  htmlFixes    = 0;   // 고친 HTML 찌꺼기 수
    bool letterSpaced = false; // 글자마다 띄어 쓴 글을 복원함
    int  wideGapBreaks = 0;  // 두 칸 이상 공백을 문단 구분으로 본 수
    int  junkLines    = 0;   // 지운 주소·반복 머리글·지정 문구 줄 수
    int  charFixes    = 0;   // 따옴표·말줄임표·한자·전각 변환 수
    int  replaceHits  = 0;   // 사용자 치환 적용 수 (정규식 포함)
    int  jamoFixes    = 0;   // 자모가 분리된 한글을 합친 수
    int  chapterBreaks = 0;  // 이어진 글 속에서 회차 제목 앞을 나눈 수
    int  sceneBreaks  = 0;   // 장면 전환으로 보고 간격을 유지한 곳 수
    bool unevenGaps   = false; // 문단 간격이 제각각이라 하나로 통일함
    bool listLike     = false; // 목록·기록형 글: 줄 그대로 둠
};

// outMap(선택): 결과의 각 글자가 원본 in의 몇 번째 글자에서 왔는지 (원본↔결과 위치 연동용)
std::u32string TidyProcess(const std::u32string& in, const TidyOptions& opt, TidyReport* rep,
                           std::vector<int>* outMap = nullptr);
