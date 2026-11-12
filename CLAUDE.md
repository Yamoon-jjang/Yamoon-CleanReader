# 야문 클린리더 — 작업 안내 (Claude용)

흐트러진 txt·EPUB의 문단을 정리하고 읽기 모드로 보여 주는 Windows 프로그램입니다.
정리 엔진은 `src/engine.cpp`(플랫폼 독립), 화면은 `src/main.cpp`(Win32)입니다.
사용자는 자기 PC에서 빌드하지 않습니다. 빌드·릴리스는 GitHub Actions(`.github/workflows/build.yml`)가 합니다.

## "이 글이 이렇게 정리된다"는 신고를 받으면

1. 받은 원문을 UTF-8 파일로 만들어 문제를 재현합니다.
   `python3 tests/run_cases.py`가 쓰는 `tests/tidy_cli.cpp`를 엔진과 함께 빌드해 쓰면 됩니다(기본 옵션).
2. 엔진을 고칩니다. 특정 글자 하나만 피하지 말고 같은 종류의 경우 전체를 다룹니다.
   애매하면 원문 줄을 그대로 두는 쪽이 안전합니다(잘못 합친 줄이 덜 정리된 줄보다 나쁨).
3. 그 글을 새 사례로 추가합니다: `tests/cases/<영문_사례이름>/input.txt`
   - 문제 부분만 **짧게** 잘라 넣습니다(공개 저장소). 소설 원문을 길게 넣지 말고, 필요하면 같은 모양의 문장으로 바꿉니다. 성인 묘사는 넣지 않습니다.
   - **보이지 않는 제로폭 글자(U+200B, U+200C, U+200D, U+2060, U+FEFF 등)는 반드시 지우고** 넣습니다.
     (사용자가 보낸 원문에서 그대로 옮기지 않음. 제로폭 처리 자체를 시험할 때는 직접 만든 짧은 글로 함)
   - `python3 tests/run_cases.py --new <사례이름>`으로 `expected.txt`를 만들고, 결과가 맞는지 눈으로 확인합니다.
4. `python3 tests/run_cases.py`로 **모든 사례**를 돌립니다. 의도하지 않은 사례가 바뀌면 고친 방법을 다시 생각합니다.
   이 도구는 공백을 뺀 글자가 원문과 결과에서 같은지도 확인합니다.
5. 나머지 검사도 통과해야 합니다.
   `python3 tests/verify_source_contracts.py` (엔진을 바꾸면 이 파일의 엔진 해시·버전 확인을 함께 고침),
   `python3 tests/compare_search_242_243.py`, `python3 tests/check_toc_lines.py`

## 지켜야 할 것

- 주 대상은 한국어 소설입니다. 한국어 정리에 어떤 문제도 생기면 안 됩니다.
- 문서 전체에서 제로폭 글자를 절대 지우지 않습니다(정리 결과에 그대로 남김).
- 읽기 모드 속도가 느려지는 수정은 하지 않습니다.
- 배포 zip에는 exe·README.txt·THIRD_PARTY_NOTICES.txt·LICENSE.txt·samples만 들어갑니다. `tests/`와 이 파일은 들어가지 않습니다.
- 매뉴얼(`dist/README.txt`, CRLF 줄바꿈 유지)과 릴리스 설명에 개발 과정을 쓰지 않고, 배포본 기준으로 씁니다.

## 버전을 올려 배포할 때

`src/main.cpp`의 `kVersion`·`kDate`, `src/app.rc`의 버전 4곳, `dist/README.txt` 머리말과 맨 끝 버전 기록,
`README.md` 맨 위 안내, `tests/verify_source_contracts.py`의 버전 확인,
`.github/release-notes/v<버전>.md`(이 파일이 있어야 main에 올렸을 때 릴리스가 만들어짐)를 함께 고칩니다.
커밋은 GitHub API로 만들어 "Verified"로 표시되게 합니다(로컬 git push로 올린 커밋은 Unverified로 보임).
