// 야문 클린리더 - EPUB 글 꺼내기 (플랫폼 독립)
#pragma once
#include <string>

// EPUB 파일 내용(bytes)에서 본문 글만 차례(spine)대로 꺼냄.
// 문단은 빈 줄로 나누고, 줄바꿈(<br>)은 줄바꿈으로 둠. 보이지 않는 서식 문자는 그대로 둠.
// decodeOther: UTF-8·UTF-16이 아닌 문서를 읽을 때 쓰는 함수 (없으면 UTF-8로 읽음)
// 실패하면 false, why에 이유 (예: "DRM으로 보호된 EPUB")
typedef std::u32string (*EpubDecodeFn)(const std::string& bytes);
bool EpubExtract(const std::string& bytes, std::u32string& out, std::wstring& why, EpubDecodeFn decodeOther = nullptr);

// 압축(deflate) 풀기 — 시험용으로도 씀. 성공하면 true
bool InflateRaw(const unsigned char* in, size_t inLen, std::string& out, size_t expect);
