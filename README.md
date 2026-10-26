<div align="center">

# 야문 클린리더

> **v2.4.3 시험 빌드**: 찾기·바꾸기 성능과 소스 구조를 보완하는 별도 테스트 브랜치입니다. 기존 v2.4.2 공식 릴리즈를 교체하지 않습니다. 변경 기록과 테스트 절차는 배포 폴더의 `README.txt` 마지막 절에 설명되어 있습니다.


**정리하고, 바로 읽는다.**

망가진 TXT·EPUB을 깨끗한 문단으로 되살리는 **정리기**와, 그 글을 책처럼 편하게 읽는 **리더기(TXT 뷰어 · 소설 뷰어)** 를<br>
하나의 프로그램에 담은 Windows 앱<br>
한국어 소설에 맞춰 다듬었고, **중국어 · 일본어 · 영어 · 유럽어 · 러시아어 등 여러 나라 글도 그 언어답게** 정리합니다.

<sub>A lightweight Windows TXT cleaner and ebook reader that restores broken line breaks into proper paragraphs — built for Korean novels and web text, with sentence and dialogue rules for many other languages. · <a href="#english">English summary</a></sub>

[![Release](https://img.shields.io/github/v/release/Yamoon-jjang/Yamoon-CleanReader?label=%EB%B0%B0%ED%8F%AC%EB%B3%B8&color=2f5aa8)](https://github.com/Yamoon-jjang/Yamoon-CleanReader/releases/latest)
![Platform](https://img.shields.io/badge/Windows-7%20%7C%208%20%7C%2010%20%7C%2011-0078D6)
![Size](https://img.shields.io/badge/%EC%84%A4%EC%B9%98-%EC%97%86%EC%9D%8C%20%C2%B7%20exe%20%ED%95%98%EB%82%98-555)
![Language](https://img.shields.io/badge/C%2B%2B17-Win32%20native-00599C)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

</div>

---

## 정리기와 리더기, 한 창에서

<table>
<tr>
<td width="50%" align="center"><b>정리 모드</b></td>
<td width="50%" align="center"><b>읽기 모드</b></td>
</tr>
<tr>
<td><img src="docs/screenshots/01-fixed-width.png" alt="정리 모드 — 고정폭으로 잘린 TXT 문단 복원"></td>
<td><img src="docs/screenshots/04-reading-sepia.png" alt="읽기 모드 — 세피아 배색 TXT 소설 뷰어"></td>
</tr>
<tr>
<td><sub>옛 모바일 txt, 한 줄로 뭉친 글, 웹에서 긁어 온 찌꺼기투성이 글을 문장 · 대사 · 장면 단위로 분석해 원래의 문단으로 되살립니다. (예: 40칸 고정폭으로 잘린 옛 모바일 글 복원)</sub></td>
<td><sub>정리한 글을 그대로 이어서 읽습니다. (예: 세피아 배색 · 글자 14 · 넓은 줄 간격)<br> 글꼴 · 크기 · 배색 · 줄 간격을 내 눈에 맞추고, 클릭 한 번으로 쪽을 넘기고, 다시 열면 읽던 곳에서 이어집니다.</sub></td>
</tr>
</table>

정리 프로그램 따로, 뷰어 따로 오갈 필요가 없습니다. 파일을 끌어다 놓으면 정리되고, **[읽기]** 버튼 하나면 정리된 글이 화면 가득 책으로 바뀝니다. 읽다가 다른 파일을 끌어다 놓아도 바로 정리해서 읽기 화면에 띄웁니다.

## 왜 클린리더인가

흐트러진 줄바꿈을 단순히 지우기만 하면 **대사가 문단에 뭉개지고, 시가 한 줄로 이어 붙고, 장 제목이 본문에 섞여 버립니다.**

클린리더는 규칙 몇 개를 기계적으로 적용하지 않습니다. 글을 열면 **먼저 글 전체를 읽고 그 글의 줄바꿈 습관부터 판별**한 다음, 그 글에 맞는 방식으로 정리합니다.

| 글의 형태 | 클린리더가 하는 일 |
|---|---|
| 빈 줄로 문단을 나눈 글 | 빈 줄을 문단 경계로 보고, 문장 중간에서 끊긴 줄(조사 · 연결 어미로 끝난 줄)만 골라 합침 |
| 옛 모바일처럼 **일정 폭으로 잘린 글** | 잘린 폭을 통계로 찾아내, 꽉 찬 줄은 잇고 짧게 끝난 줄 · 들여쓴 줄에서 문단을 나눔 |
| **한 줄로 이어진 글** | 문장 단위로 나누고, 글 속에 파묻힌 회차 제목(`… 끝났다. 제2화 만남 아침이 …`)을 찾아 제목과 본문을 분리 |
| 줄마다 빈 줄이 낀 글 · 간격이 제각각인 글 | 간격을 하나로 통일하고, 가끔만 넓게 띄운 곳은 **장면 전환**으로 인식 |
| 일부분만 끊긴 글 | 멀쩡한 글 사이에 끊긴 부분이 섞여 있어도 그 부분만 찾아 이음 (앞 · 중간 · 끝, 여러 군데, 파일 크기와 상관없이) |
| 대화 기록 · 대본 · 접속 기록 같은 **목록형 글** | 한 줄을 한 항목으로 보고 합치지 않음 |
| 작가가 일부러 짧게 끊은 **시 · 가사 · 편지** | 합치지 않고 원래 모양 그대로 보존 |

여러 줄에 걸친 대사는 닫는 따옴표까지 한 덩어리로 묶고, 말줄임표 · 문장 속 인용 · 괄호 안에서는 문장을 나누지 않으며, `1,000`이나 `3.14` 같은 숫자와 `Mr.` 같은 약어도 구별합니다.

## 여러 나라 글도 그 언어답게

나라마다 문장을 끝내는 부호, 대사를 쓰는 방식, 약어 습관이 다릅니다. 한국어 규칙을 그대로 들이대면 중국어 글은 한 문장도 나뉘지 않고, 러시아어 대사는 말꼬리가 뚝뚝 떨어져 나갑니다.

클린리더는 **문단마다 어느 문자로 쓴 글인지 먼저 보고**, 그 언어의 규칙으로 정리합니다. 그래서 번역 소설처럼 한국어 글 속에 원문 문단이 섞여 있어도 각각 제대로 정리되고, 한국어 문장에 한자나 `。` 가 조금 섞인 정도로는 한국어 정리 결과가 달라지지 않습니다.

| 언어 | 문장 나누기 | 대사 · 따옴표 | 옛 인코딩 자동 판별 |
|---|---|---|---|
| **한국어** | 어미 · 조사까지 보는 전용 규칙 | `" "` `' '` `「 」` `『 』` · 여러 줄 대사 | CP949(EUC-KR) · 조합형 |
| **중국어** (간체 · 번체) | 띄어쓰기 없는 `。！？`, 반각 `. ! ?` | `“ ”` `「 」` · 대사 뒤 서술은 붙여 둠 | GBK · Big5 |
| **일본어** | 띄어쓰기 없는 `。！？`, `。。。` 말줄임은 그대로 | `「 」` `『 』` | Shift-JIS |
| **영어** | `Mr.` `a.m.` `U.S.A.` `Jan. 3` 같은 약어 구별 | `" "` `' '` · `"Yes." It was…` 다음 문장 구분 | Windows-1252 |
| **프랑스어 · 독일어 · 스페인어 · 이탈리아어 · 포르투갈어** | 각국 약어 · 날짜(`am 3. Oktober`) 구별 | `« »` `„ “` `»…«` · 스페인식 `—¿…? —preguntó—` | Windows-1252 |
| **폴란드어 · 체코어 · 헝가리어 · 터키어 · 발트어** | `Np.` `godz. 5` 같은 약어 구별 | `„ “` · 줄표 `—` 대사 | Windows-1250 · 1254 · 1257 |
| **러시아어 · 우크라이나어 등 키릴 문자** | `т.е.` 같은 약어 구별, `…` 뒤 새 문장 | 줄표 대사(`— Да! — сказал он.`)를 한 덩어리로 | Windows-1251 · KOI8-R |
| **그리스어 · 베트남어** | 그리스어 물음표 `;`, 확장 라틴 대문자 인식 | 줄표 대사 | Windows-1253 · 1258 |
| **아랍어 · 히브리어 · 힌디어 · 미얀마어** | `؟` `।` `॥` `။` 같은 그 언어의 문장 끝 부호 | `« »` `" "` (오른쪽→왼쪽 글자도 바르게 표시) | Windows-1256 · 1255 |
| **태국어** | 문장 끝 부호가 없는 언어라 줄바꿈을 그대로 존중 | — | Windows-874 |

라틴 · 키릴 · 그리스 문자의 문장은 대문자로 시작한다는 점을 이용해, 언어마다 약어 목록을 다 갖추지 않아도 `p. m.` `Np.` `т.зв.` 처럼 처음 보는 약어에서 문장을 잘못 끊지 않습니다. UTF-8 · UTF-16 글은 언어와 상관없이 그대로 열리고, 자동 판별이 틀리면 옵션 **[읽기]** 에서 인코딩을 직접 고를 수 있습니다.

> 중국어 · 일본어 · 영어 원문을 번역해 읽는다면, 함께 만든 **[야문 Novel Translator](https://github.com/Yamoon-jjang/Yamoon-Novel-Translator)** 로 옮긴 뒤 버튼 하나로 클린리더에서 정리해 읽을 수 있습니다.

## 한눈에 보기

### 한국어 특수 문제를 한 번에

<img src="docs/screenshots/02-korean-cleanup.png" alt="한국어 TXT 정리 — 분리된 자모 결합, 반복 머리글 제거" width="100%">

<sub>맥에서 만든 파일의 <b>분리된 자모 190자 결합</b>, 페이지마다 반복되는 머리글 · 광고 · 주소 31줄 제거, 괄호 속 한자 삭제, 전각 → 반각 변환을 한 번에 — 상태줄에 고친 내용이 그대로 표시됩니다</sub>

### 웹에서 긁어 온 글도 깨끗하게 (어두운 테마)

<img src="docs/screenshots/03-messy-web-text-dark.png" alt="웹에서 복사한 지저분한 글 정리, 어두운 테마" width="100%">

<sub>문장부호 앞 공백 · 따옴표 안쪽 공백 36곳, 글자마다 띄어 쓴 글(<code>안 녕 하 세 요</code>) 복원, 쪽 번호 줄 제거, <code>&amp;nbsp;</code> <code>&amp;quot;</code> <code>&lt;br&gt;</code> 같은 HTML 찌꺼기 정리, 한 글자만 다음 줄로 넘어간 <code>뻗었 / 다 / .</code> 합치기</sub>

### 읽기 모드 — 오래 읽어도 편하게

<img src="docs/screenshots/05-reading-night.png" alt="읽기 모드, 저녁 앰버 배색과 어두운 테마" width="100%">

<sub>읽기 모드 · 저녁(앰버) 배색 · 어두운 테마</sub>

- **쪽 넘김** — 글의 아래쪽·오른쪽 끝을 클릭하면 다음 쪽, 위쪽·왼쪽 끝을 클릭하면 앞쪽. 화면 맨 아래 줄이 다음 쪽의 맨 위 줄이 되어 읽던 줄을 놓치지 않습니다. Space · Page Down · → 키도 됩니다.
- **읽던 곳 기억** — 파일마다 읽던 위치를 기억해, 다시 열면 그곳으로 돌아갑니다. 지금까지 읽은 양은 %로 표시됩니다.
- **목차 · 책갈피 · 찾기** — "제1화", "프롤로그" 같은 회차 제목을 찾아 목차를 만들고, 파일마다 책갈피를 남깁니다.
- **눈이 편한 화면** — 배색 8가지(아이보리 · 세피아 · 보호 녹색 · 저녁 앰버 · 어두운 화면 …)를 색 견본으로 보고 고르기, 사용자 지정 색, 글꼴 · 크기 · 굵게 · 줄 간격(좁게~아주 넓게) · 여백 조절, Ctrl+휠로 글자 크기. 아이콘 버튼에 마우스를 올리면 이름이 나옵니다.
- **전체 화면(F11)** — 도구줄을 모두 숨기고 글만 화면 가득. 큰 글도 처음부터 끝까지 이어서 읽습니다.

## 주요 기능

**정리 엔진**
- 줄바꿈 습관 자동 판별 (빈 줄형 · 줄바꿈형 · 고정폭 · 한 줄형 · 목록형)
- 글자 단위로 잘린 줄(`익 / 숙한`)은 붙이고, 단어 단위로 잘린 줄은 띄어서 합치기
- 서술 중간에 박힌 대사를 새 줄로 떼기, 대사 뒤 같은 줄의 서술은 붙여 두기
- 회차 · 장 제목 인식 — `제1화`, `[2화]`, `【3장】`, `<외전>`, `《프롤로그》` — 수량 표현(`2장 분량의`)과는 구별
- 꾸밈 제목(`──────` / 제목 / `──────`), 장면 구분선, 가운데 정렬 머리말 보존
- 글 끝 덧붙임(`PS:`, `작가의 말`, `후기:`) 앞에서 문단 나누기
- 다국어 정리 — 문단마다 언어를 판단해 그 언어의 문장 · 대사 규칙으로 정리 ([여러 나라 글도 그 언어답게](#여러-나라-글도-그-언어답게))

**다듬기**
- 연속 공백 · 전각 공백 · 탭 · 한글 채움 문자 정리, 문장부호 앞 공백 제거, 빠진 공백 넣기
- `ㅋㅋ` · `ㅎㅎ` · `ㅠㅠ`처럼 문장 끝에 붙인 말은 앞 문장에 붙여 두기
- 분리된 자모 결합, 글자마다 띄어 쓴 글 복원, 괄호 속 한자 삭제, 전각 → 반각
- 쪽 번호 · 반복 머리글 · 광고 · 주소 줄 · HTML 찌꺼기 제거, 사용자 규칙(정규식 포함)

**파일**
- 인코딩 자동 판별 — UTF-8 · UTF-16(LE/BE, BOM 유무) · CP949/EUC-KR · 조합형(Johab) · Shift-JIS · GBK/GB2312 · Big5 · Windows-1250~1258 · KOI8-R · Windows-874 — 저장할 때도 같은 인코딩으로
- 확장자가 아니라 **파일 내용으로** 텍스트인지 확인 (옛 `.cap` 파일도 열림)
- **EPUB 전자책** — 본문 글을 차례대로 꺼내 정리 · 읽기 (DRM이 걸린 책 제외, 추가 프로그램 없이 직접 처리)
- 최대 100MB, 여러 파일 한꺼번에 변환, 원본은 그대로 두고 `_정리.txt`로 따로 저장
- 원본 칸에서 클릭한 곳이 결과 칸의 같은 위치로 따라가는 연동 보기
- 찾기 · 바꾸기 — 여러 줄 칸으로 빈 줄 개수까지 바꾸기. 같은 조건의 반복 검색 결과 재사용으로 지연을 줄임. **큰 파일의 정리 미리보기에서는 바꾸기 버튼을 누르면 먼저 동의를 받고, 승인 시 검색 조건을 유지한 채 읽기 모드로 전환해 작업을 수행**
- 최근 파일 20개 — 몇 %까지 읽었는지 함께 표시. 경로가 바뀌어도 회색으로 막지 않고, 클릭하여 새 위치에서 다시 선택 가능

**읽기**
- 글꼴 · 크기(9~28) · 굵게 · 줄 간격(좁게~아주 넓게) · 여백, 배색 8가지(아이보리 · 세피아 · 보호 녹색 · 저녁 앰버 · 어두운 화면 …) + 사용자 지정 색
- 클릭 · Space 쪽 넘김, 읽던 위치 기억, 목차, 파일별 책갈피, 전체 화면, Ctrl+휠 글자 크기. 일반 휠은 Windows의 스크롤 줄 수 설정을 따르면서 즉시 이동하여 고속 휠의 잔여 스크롤 문제를 개선
- 밝은 · 어두운 테마(시스템 설정 따라가기 포함), 고해상도(DPI) 화면 대응
- 창 위치 · 크기 기억 — 다시 켜면 마지막에 놓아둔 자리에 그대로 열림 (보조 모니터 포함)

## 기술 사양

| | |
|---|---|
| 구현 | C++17, Win32 API 직접 사용 (프레임워크 없음) |
| 실행 파일 | exe 하나, 약 1.5MB, **설치 불필요** |
| 의존성 | 런타임 정적 링크 — Windows 기본 DLL 외에 아무것도 필요 없음 |
| 네트워크 | **인터넷 연결 기능 없음** — 고른 파일을 열고 저장하는 일만 함 |
| 보안 | DEP · ASLR(High-Entropy) 적용, 관리자 권한 요청 안 함, 실행 파일 압축(패킹) 없음 |
| 빌드 | MinGW-w64 GCC 13 — GitHub Actions가 빌드할 때마다 가져오는 DLL · 보안 플래그 · 버전 정보를 자동 검사 |
| 지원 | Windows 7 이상 (64비트) |

## 내려받기와 실행

1. **[Releases](https://github.com/Yamoon-jjang/Yamoon-CleanReader/releases/latest)** 에서 최신 zip을 받아 원하는 폴더에 풉니다.
2. `YamoonCleanReader.exe`를 실행합니다. Windows "PC 보호" 창이 나오면 **추가 정보 → 실행**을 누릅니다. (코드 서명이 없는 개인 제작 프로그램이라 나오는 안내입니다)
3. TXT 파일이나 EPUB 전자책을 창에 끌어다 놓으면 바로 정리됩니다. **[저장]** 을 누르면 원본 옆에 `파일명_정리.txt`가 생깁니다.

자세한 사용법은 함께 들어 있는 `README.txt`, 바로 시험해 볼 수 있는 예제는 `samples` 폴더에 있습니다.

> **새 버전으로 바꿀 때** — 설정(창 위치 · 크기, 옵션, 보기 설정, 최근 파일, 읽던 곳)은 exe 옆 `YamoonCleanReader.ini`에 저장됩니다. 새 버전을 다른 폴더에 풀었다면 예전 폴더의 이 파일을 새 폴더로 복사해 두세요. 예전 폴더를 그냥 지우면 설정이 처음 상태로 돌아갑니다.

## 소스에서 빌드하기

명령은 [BUILD.txt](BUILD.txt)에 있습니다. 정보 창의 **빌드** 줄에 빌드 시각과 커밋이 표시되어, 같은 버전이라도 어느 빌드인지 구분할 수 있습니다.

---

<a id="english"></a>

## English

**Yamoon CleanReader** is a lightweight Windows app that combines a **TXT cleaner** and an **ebook reader** in one window. It repairs broken line breaks in TXT and EPUB files and restores proper paragraphs, then lets you read the result like a book. It is designed mainly for **Korean novels and web text** (the interface is in Korean), but it also cleans text in many other languages by their own rules.

Instead of blindly removing line breaks, it first analyzes how the whole file breaks its lines, then cleans it accordingly.

- **Paragraph restoration** — detects blank-line, hard-wrapped, fixed-width (old mobile TXT), single-line and list-style text; keeps multi-line dialogue together and leaves poems, lyrics and letters intact
- **Chapter detection** — finds chapter titles even when buried inside a single long line, and builds a table of contents
- **Korean-specific fixes** — joins decomposed jamo (NFD text from macOS), restores text spaced out letter by letter, removes Hanja in parentheses, converts full-width to half-width
- **Web text cleanup** — removes page numbers, repeated headers, ads and HTML leftovers (`&nbsp;`, `<br>`), plus custom regex rules
- **Multilingual** — the script of each paragraph is detected first, then the matching rules are applied: Chinese and Japanese sentences without spaces (`。！？`), abbreviations in English and European languages (`Mr.`, `p. m.`, `Np.`, `т.е.`), quotes such as `« »`, `„ “`, `»…«` and dash dialogue (`— Да! — сказал он.`), plus Greek `;`, Arabic `؟`, Hindi `।` and Burmese `။`; Thai line breaks are kept. Mixed-language files work, and Korean results are unaffected
- **Encoding auto-detection** — UTF-8, UTF-16, CP949/EUC-KR, Johab, Shift-JIS, GBK, Big5, Windows-1250 to 1258, KOI8-R, Windows-874
- **Reading mode** — click or Space to turn pages, remembers your reading position and the window position and size, bookmarks, 8 color schemes, adjustable font, size, line spacing and margins, full-screen mode, dark theme
- **EPUB support** (DRM-free), batch conversion, files up to 100 MB; originals are never modified

Native C++17 / Win32, a single ~1.5 MB exe — no installation, no runtime dependencies, no network access. Windows 7 or later (64-bit). Download from [Releases](https://github.com/Yamoon-jjang/Yamoon-CleanReader/releases/latest).

Companion app: **[Yamoon Novel Translator](https://github.com/Yamoon-jjang/Yamoon-Novel-Translator)** translates Chinese, Japanese and English novels into Korean.

---

## 라이선스

[MIT License](LICENSE) — 누구나 무료로 사용 · 수정 · 재배포할 수 있으며, 저작권 표시는 유지해야 합니다.
실행 파일에 포함된 런타임 구성 요소의 고지는 [dist/THIRD_PARTY_NOTICES.txt](dist/THIRD_PARTY_NOTICES.txt)에 있습니다.

<div align="center"><sub>제작 · 야문 짱 &amp; Claude (협동작)</sub></div>
