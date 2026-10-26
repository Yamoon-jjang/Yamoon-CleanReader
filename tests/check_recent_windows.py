#!/usr/bin/env python3
"""Compile the actual recent-file functions and exercise them on native Windows.

This is not a reimplementation: extract the functions from src/main.cpp,
compile them against Windows profile/file APIs, then execute an actual
ANSI-INI -> lossless Unicode path -> reopen workflow, plus ambiguous repairs.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "src/main.cpp").read_text(encoding="utf-8")
names = [
    "RecentEncodePath", "RecentDecodePath", "RecentIniValue", "LoadRecent",
    "SaveRecent", "RecentMaskedNameMatch", "RecoverDamagedRecentPath", "AddRecent",
]

def extract(name):
    rx = re.compile(r"(?m)^static\s+[^\n]+\s+" + re.escape(name) + r"\s*\(")
    m = rx.search(src)
    assert m, f"Function absent: {name}"
    start = m.start()
    opening = src.index("{", m.end())
    depth, end = 1, opening+1
    while depth and end < len(src):
        if src[end] == "{": depth += 1
        if src[end] == "}": depth -= 1
        end += 1
    assert depth == 0
    return src[start:end] + "\n"

# Preserve the order of declarations: LoadRecent -> SaveRecent -> recovery -> AddRecent.
functions = "\n".join(extract(n) for n in names)
cpp = r"""
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cwctype>
#include <iostream>
#include <string>
#include <vector>
using std::wstring;
static wstring g_ini;
static bool FileExists(const wstring& p) {
    DWORD a=GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static void MakeFile(const wstring& p) {
    HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(h!=INVALID_HANDLE_VALUE);
    DWORD n=0;
    assert(WriteFile(h,"demo",4,&n,nullptr) && n==4);
    CloseHandle(h);
}
""" + functions + r"""
int wmain() {
    wchar_t base[MAX_PATH] = {};
    assert(GetTempPathW(MAX_PATH,base)>0);
    wstring dir = wstring(base)+L"YamoonRecentUnicode_"+std::to_wstring(GetCurrentProcessId())+L"\\";
    assert(CreateDirectoryW(dir.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS);
    g_ini = dir+L"YamoonCleanReader.ini";
    const char legacy[]="[opt]\r\nvsize=13\r\n[pos]\r\n123456=123,456\r\n[recent]\r\nr0=missing.txt\r\n";
    HANDLE h=CreateFileW(g_ini.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(h!=INVALID_HANDLE_VALUE);
    DWORD written=0;
    assert(WriteFile(h,legacy,sizeof(legacy)-1,&written,nullptr) && written==sizeof(legacy)-1);
    CloseHandle(h);

    // Legacy ANSI INI entries must remain readable until opening an actual
    // file migrates the path. Do not re-encode all unrelated INI settings.
    auto old=LoadRecent();
    assert(old.size()==1 && old[0]==L"missing.txt");
    const wstring cjk = dir+L"猫犬虎.txt";
    const wstring mix = dir+L"公怨落之九尾天狐_한글_日本語_😀.txt";
    MakeFile(cjk);MakeFile(mix);

    // Emulate a pre-fix INI entry whose unrepresentable characters became '?'.
    assert(WritePrivateProfileStringW(L"recent",L"r0",(dir+L"???.txt").c_str(),g_ini.c_str()));
    const auto corrupted=LoadRecent();
    assert(corrupted.size()==1 && !FileExists(corrupted[0]));
    assert(RecoverDamagedRecentPath(corrupted[0])==cjk);

    // Real profile API is used. Encoded values are 7-bit ASCII in an ANSI file,
    // so Windows code-page fallback cannot alter paths when writing.
    SaveRecent({cjk,mix});
    const auto paths=LoadRecent();
    assert(paths.size()==2 && paths[0]==cjk && paths[1]==mix);
    assert(FileExists(paths[0]) && FileExists(paths[1]));
    const auto raw0=RecentIniValue(0),raw1=RecentIniValue(1);
    assert(raw0.find(L"u16:")==0 && raw1.find(L"u16:")==0);
    for(wchar_t c:raw0+raw1) assert(c < 128);
    assert(GetPrivateProfileIntW(L"opt",L"vsize",0,g_ini.c_str())==13);
    assert(GetPrivateProfileIntW(L"pos",L"123456",0,g_ini.c_str())==123);

    // Reopen another file: AddRecent must preserve Unicode after a save + reload.
    const wstring another=dir+L"汉字_あいう_한글_😃.txt";
    MakeFile(another);
    AddRecent(another);
    const auto after=LoadRecent();
    assert(after.size()==3 && after[0]==another && after[1]==cjk && after[2]==mix);
    AddRecent(mix);
    const auto promoted=LoadRecent();
    assert(promoted.size()==3 && promoted[0]==mix && promoted[1]==another && promoted[2]==cjk);

    // A damaged name is recoverable only if exactly one file can match.
    const wstring similar=dir+L"龙虎龙.txt";
    MakeFile(similar);
    assert(RecoverDamagedRecentPath(dir+L"???.txt").empty());
    assert(RecoverDamagedRecentPath(dir+L"????.txt").empty());
    assert(RecentMaskedNameMatch(L"???.txt", L"猫犬虎.txt"));
    assert(!RecentMaskedNameMatch(L"???.txt", L"猫犬虎xx.txt"));

    // Invalid persisted encoding never returns a truncated/partially decoded path.
    wstring p=L"unchanged";
    assert(!RecentDecodePath(L"u16:",p) && p==L"unchanged");
    assert(!RecentDecodePath(L"u16:00GG",p) && p==L"unchanged");
    assert(!RecentDecodePath(L"u16:0000",p) && p==L"unchanged");
    assert(!RecentDecodePath(L"u16:003",p) && p==L"unchanged");
    assert(RecentDecodePath(L"old_filename.txt",p) && p==L"old_filename.txt");

    // Maximum normal 520-unit Windows path must not truncate the profile buffer.
    const wstring longpath=L"C:\\"+wstring(490,L'猫')+L".txt";
    SaveRecent({longpath});
    assert(LoadRecent().size()==1 && LoadRecent()[0]==longpath);
    assert(RecentIniValue(0).size()>1024);

    DeleteFileW(g_ini.c_str());
    DeleteFileW(cjk.c_str());DeleteFileW(mix.c_str());
    DeleteFileW(another.c_str());DeleteFileW(similar.c_str());
    RemoveDirectoryW(dir.c_str());
    std::cout<<"PASS native Windows ANSI-INI Unicode persistence, 4 scripts, emoji,"
             <<" reopen, long entry, unique/ambiguous legacy recovery, settings preservation\n";
}
"""
with tempfile.TemporaryDirectory() as td:
    work = Path(td)
    source = work / "recent_test.cpp"
    exe = work / "recent_test.exe"
    source.write_text(cpp, encoding="utf-8")
    subprocess.run(
        ["cl", "/nologo", "/std:c++17", "/EHsc", "/utf-8", str(source), "/Fe:"+str(exe)],
        cwd=work, check=True, timeout=100)
    subprocess.run([str(exe)], cwd=work, check=True, timeout=100)
