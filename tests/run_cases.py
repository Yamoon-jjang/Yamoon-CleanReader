#!/usr/bin/env python3
"""정리 엔진 회귀 시험 모음.

tests/cases/<사례>/input.txt 를 기본 옵션(사례 폴더에 options.txt가 있으면 그 옵션)으로 정리한 결과가
tests/cases/<사례>/expected.txt 와 글자 하나까지 같은지 확인합니다.
input.txt는 UTF-8로 저장합니다(앱이 하는 인코딩 판별은 이 시험의 대상이 아님).
엔진(src/engine.cpp)을 고칠 때마다 반드시 돌립니다. GitHub 빌드에서도 exe를 만들기 전에 돌립니다.

  python3 tests/run_cases.py            모든 사례 확인 (하나라도 다르면 실패)
  python3 tests/run_cases.py --new NAME  input.txt만 넣은 새 사례의 expected.txt를 지금 결과로 만듦
                                         (만든 결과를 눈으로 확인한 뒤 올릴 것)

또, 모든 사례에서 공백·줄바꿈을 뺀 글자가 원문과 결과에서 같은지 확인합니다
(정리하다 글자가 사라지거나 생기는 사고 방지).
"""
from pathlib import Path
import difflib
import subprocess
import sys
import tempfile
import unicodedata

root = Path(__file__).resolve().parents[1]
cases = root / "tests" / "cases"


def build(td):
    exe = Path(td) / "tidy_cli"
    subprocess.run(["g++", "-std=c++17", "-O2", str(root / "tests" / "tidy_cli.cpp"),
                    str(root / "src" / "engine.cpp"), "-o", str(exe)], check=True)
    return exe


def tidy(exe, data, opts=()):
    return subprocess.run([str(exe), *opts], input=data, capture_output=True, check=True, timeout=120).stdout


def options(d):
    # 사례 폴더에 options.txt가 있으면 그 옵션으로 정리 (예: --junk). 없으면 기본 옵션
    f = d / "options.txt"
    return f.read_text(encoding="utf-8").split() if f.exists() else []


def solid(b):
    # 자모가 분리된 한글(맥 파일)을 합치는 것은 정상 동작이므로 NFC로 맞춘 뒤 비교
    t = unicodedata.normalize("NFC", b.decode("utf-8", "replace").lstrip("\ufeff"))
    return "".join(ch for ch in t if not ch.isspace() and ch not in "\u3164\uffa0\u2800")


def main():
    new = sys.argv[2] if len(sys.argv) > 2 and sys.argv[1] == "--new" else None
    with tempfile.TemporaryDirectory() as td:
        exe = build(td)
        if new:
            d = cases / new
            out = tidy(exe, (d / "input.txt").read_bytes(), options(d))
            (d / "expected.txt").write_bytes(out)
            print(out.decode("utf-8"))
            print(f"[만듦] {d / 'expected.txt'} — 결과가 맞는지 확인하세요")
            return 0
        fails = 0
        dirs = sorted(p for p in cases.iterdir() if p.is_dir())
        for d in dirs:
            src = (d / "input.txt").read_bytes()
            want = (d / "expected.txt").read_bytes()
            opts = options(d)
            got = tidy(exe, src, opts)
            if got != want:
                fails += 1
                print(f"[실패] {d.name}")
                diff = difflib.unified_diff(want.decode("utf-8").splitlines(), got.decode("utf-8").splitlines(),
                                            "expected", "now", lineterm="")
                print("\n".join(list(diff)[:60]))
            if not opts and solid(src) != solid(got):  # (줄을 지우는 옵션을 켠 사례는 글자 수가 줄어드는 것이 정상)
                fails += 1
                print(f"[실패] {d.name}: 공백을 뺀 글자가 원문과 다름 (글자가 사라지거나 생김)")
        print(f"정리 엔진 시험 {len(dirs)}개 중 실패 {fails}개")
        return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
