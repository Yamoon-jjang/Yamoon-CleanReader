// ---------------------------------------------------------------------------
// 찾기·바꾸기 창 (정리 결과 칸의 글을 직접 찾아 바꿈)
// 보이지 않는 서식 문자는 찾을 때 건너뛰고, 바꿀 때도 그 자리에 그대로 남김
static HWND g_rpFind, g_rpRepl, g_rpFindX, g_rpReplX, g_rpInfo;
static bool g_rpExpanded = false;
// 창을 닫았다 열어도 찾을 말·바꿀 말·옵션·기록은 그대로 (프로그램을 켜 둔 동안)
static wstring g_rpLastFind, g_rpLastRepl;
static std::vector<wstring> g_rpHistF, g_rpHistR;
static bool g_rpOpt[4] = {false, false, false, false};
static void RpSaveState() {
    auto txt = [](HWND c) { int n = GetWindowTextLengthW(c); wstring w(n + 1, 0); GetWindowTextW(c, &w[0], n + 1); w.resize(n); return w; };
    auto hist = [](HWND c) {
        std::vector<wstring> v;
        int n = (int)SendMessageW(c, CB_GETCOUNT, 0, 0);
        for (int i = 0; i < n; ++i) {
            int l = (int)SendMessageW(c, CB_GETLBTEXTLEN, i, 0);
            if (l <= 0) continue;
            wstring w(l + 1, 0);
            SendMessageW(c, CB_GETLBTEXT, i, (LPARAM)&w[0]);
            w.resize(l);
            v.push_back(w);
        }
        return v;
    };
    g_rpLastFind = txt(g_rpFind); g_rpLastRepl = txt(g_rpRepl);
    g_rpHistF = hist(g_rpFind); g_rpHistR = hist(g_rpRepl);
    int ids[] = {ID_RP_CASE, ID_RP_WORD, ID_RP_UP, ID_RP_REGEX};
    for (int i = 0; i < 4; ++i) g_rpOpt[i] = Checked(GetDlgItem(g_rpWnd, ids[i]));
}
static void RpLoadState(HWND h) {
    for (auto& w : g_rpHistF) SendMessageW(g_rpFind, CB_ADDSTRING, 0, (LPARAM)w.c_str());
    for (auto& w : g_rpHistR) SendMessageW(g_rpRepl, CB_ADDSTRING, 0, (LPARAM)w.c_str());
    SetWindowTextW(g_rpFind, g_rpLastFind.c_str());
    SetWindowTextW(g_rpRepl, g_rpLastRepl.c_str());
    int ids[] = {ID_RP_CASE, ID_RP_WORD, ID_RP_UP, ID_RP_REGEX};
    for (int i = 0; i < 4; ++i) SetToggle(GetDlgItem(h, ids[i]), g_rpOpt[i]);
}
static wstring g_rpUndoBefore, g_rpUndoAfter;  // 모두 바꾸기 되돌리기 (Ctrl+Z)
struct RpHit { size_t a, b; wstring rep; };
// All matches are stored in ascending source order. Reuse that order for O(log N)
// navigation rather than rescanning up to 200,000 matches on every Find click.
static size_t RpHitFrom(const std::vector<RpHit>& hits, size_t position) {
    return (size_t)(std::lower_bound(hits.begin(), hits.end(), position,
        [](const RpHit& h, size_t pos) { return h.a < pos; }) - hits.begin());
}
static bool RpWordCh(wchar_t c) {
    return iswalnum(c) || c == L'_' || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3131 && c <= 0x318E) || (c >= 0x4E00 && c <= 0x9FFF) ||
           (c >= 0x3040 && c <= 0x30FF);
}
static wchar_t RpFold(wchar_t c) { return (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : (c >= 0xFF21 && c <= 0xFF3A) ? (wchar_t)(c + 32) : c; }
static wstring RpGet(HWND combo, HWND multi) {
    HWND e = g_rpExpanded ? multi : combo;
    int n = GetWindowTextLengthW(e);
    wstring s(n + 1, 0);
    GetWindowTextW(e, &s[0], n + 1);
    s.resize(n);
    wstring o;  // 줄 끝은 칸 안과 같이 \r 하나로
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r' && i + 1 < s.size() && s[i + 1] == L'\n') { o += L'\r'; ++i; }
        else if (s[i] == L'\n') o += L'\r';
        else o += s[i];
    }
    return o;
}
// 결과 칸의 글 (칸 안의 위치와 같게 줄 끝은 \r)
static wstring RpHay() {
    wstring w = GetRichText(g_dst), o;
    o.reserve(w.size());
    for (size_t i = 0; i < w.size(); ++i) {
        if (w[i] == L'\r' && i + 1 < w.size() && w[i + 1] == L'\n') { o += L'\r'; ++i; }
        else o += w[i];
    }
    return o;
}
static bool RpSearch(const wstring& hay, std::vector<RpHit>& hits, wstring& err) {
    hits.clear();
    wstring q = RpGet(g_rpFind, g_rpFindX), repl = RpGet(g_rpRepl, g_rpReplX);
    wstring q2;
    for (wchar_t c : q) if (!IsInvisibleCh((char32_t)c)) q2 += c;
    q = q2;
    if (q.empty()) { err = L"찾을 말을 적어 주세요."; return false; }
    bool cs = Checked(GetDlgItem(g_rpWnd, ID_RP_CASE)), word = Checked(GetDlgItem(g_rpWnd, ID_RP_WORD));
    bool rx = Checked(GetDlgItem(g_rpWnd, ID_RP_REGEX));
    // 일반 글에는 보이지 않는 서식 문자가 거의 없다. 이때는 별도 위치표(글자당 size_t)를 만들지 않는다.
    bool stripped = false;
    for (wchar_t c : hay) if (IsInvisibleCh((char32_t)c)) { stripped = true; break; }
    wstring filtered;
    std::vector<size_t> vp;
    if (stripped) {
        filtered.reserve(hay.size()); vp.reserve(hay.size());
        for (size_t i = 0; i < hay.size(); ++i)
            if (!IsInvisibleCh((char32_t)hay[i])) { filtered += hay[i]; vp.push_back(i); }
    }
    const wstring& vis = stripped ? filtered : hay;
    auto wordOk = [&](size_t s, size_t e) {
        if (!word) return true;
        if (s > 0 && RpWordCh(vis[s - 1]) && RpWordCh(vis[s])) return false;
        if (e < vis.size() && RpWordCh(vis[e]) && RpWordCh(vis[e - 1])) return false;
        return true;
    };
    auto add = [&](size_t s, size_t e, const wstring& rep) {
        RpHit h; h.a = stripped ? vp[s] : s; h.b = stripped ? vp[e - 1] + 1 : e; h.rep = rep;
        if (stripped) for (size_t k = h.a; k < h.b; ++k)
            if (IsInvisibleCh((char32_t)hay[k])) h.rep += hay[k];  // 서식 문자는 남김
        hits.push_back(h);
    };
    const size_t kMax = 200000;
    if (!rx) {
        wstring V, Q = q;
        const wstring* searchText = &vis;
        if (!cs) {
            V = vis;
            for (auto& c : V) c = RpFold(c);
            for (auto& c : Q) c = RpFold(c);
            searchText = &V;
        }
        size_t pos = 0;
        while (hits.size() < kMax) {
            size_t k = searchText->find(Q, pos);
            if (k == wstring::npos) break;
            if (wordOk(k, k + Q.size())) { add(k, k + Q.size(), repl); pos = k + Q.size(); }
            else pos = k + 1;
        }
        return true;
    }
    try {
        auto fl = std::regex::ECMAScript | (cs ? std::regex::flag_type(0) : std::regex::icase);
        // 정규식에 줄바꿈(\n 또는 여러 줄 칸의 줄바꿈)이 있으면 글 전체에서, 없으면 한 줄씩 찾음
        bool multi = q.find(L'\r') != wstring::npos || q.find(L"\\n") != wstring::npos;
        for (auto& c : q) if (c == L'\r') c = L'\n';
        std::wregex re(q, fl);
        auto fixRep = [](wstring r) {  // 바꿀 말의 \n 은 줄바꿈
            for (size_t k = 0; (k = r.find(L"\\n", k)) != wstring::npos;) r.replace(k, 2, L"\r");
            return r;
        };
        // 긴 글은 나눠서 찾음 (정규식 엔진이 아주 긴 글에서 멈추지 않도록)
        // 여러 줄: 빈 줄 묶음이 갈리지 않게 "줄바꿈 다음 글자" 자리에서 나눔 / 한 줄: 긴 줄은 공백에서 나눔
        // One-line patterns can scan vis directly; no full-document duplicate or
        // per-chunk temporary substrings are needed for regex iteration.
        wstring multiline;
        if (multi) {
            multiline = vis;
            for (auto& c : multiline) if (c == L'\r') c = L'\n';
        }
        const wstring& all = multi ? multiline : vis;
        auto run = [&](size_t base, size_t len) {
            auto first = all.cbegin() + (std::ptrdiff_t)base;
            auto last = first + (std::ptrdiff_t)len;
            for (std::wsregex_iterator it(first, last, re), end; it != end && hits.size() < kMax; ++it) {
                const std::wsmatch& m = *it;
                if (m.length(0) == 0) continue;
                size_t s = base + (size_t)m.position(0), e = s + (size_t)m.length(0);
                if (!wordOk(s, e)) continue;
                if ((vis[s] >= 0xDC00 && vis[s] <= 0xDFFF) || (vis[e - 1] >= 0xD800 && vis[e - 1] <= 0xDBFF)) continue;  // 이모지 반쪽
                wstring r = m.format(repl);
                for (auto& c : r) if (c == L'\n') c = L'\r';
                add(s, e, fixRep(r));
            }
        };
        const size_t kChunk = 2000;
        if (multi) {
            size_t pos = 0, N = all.size();
            while (pos < N && hits.size() < kMax) {
                size_t end = std::min(N, pos + kChunk * 2);
                if (end < N) {
                    size_t k = end, lim = std::min(N, end + kChunk * 2);
                    while (k < lim && !(all[k - 1] == L'\n' && all[k] != L'\n')) ++k;
                    if (k < lim) end = k;
                }
                run(pos, end - pos);
                pos = end;
            }
            return true;
        }
        size_t ls = 0;
        while (ls <= vis.size() && hits.size() < kMax) {  // 한 줄씩 (^ $ 가 줄 처음·끝)
            size_t le = vis.find(L'\r', ls);
            if (le == wstring::npos) le = vis.size();
            size_t pos = ls;
            while (pos < le) {
                size_t end = le;
                if (le - pos > kChunk) {
                    end = pos + kChunk;
                    size_t k = end;
                    while (k > pos + kChunk / 2 && vis[k - 1] != L' ') --k;
                    if (k > pos + kChunk / 2) end = k;
                }
                run(pos, end - pos);
                pos = end;
            }
            ls = le + 1;
        }
    } catch (...) {
        err = L"정규식 문법이 맞지 않거나 너무 복잡합니다.";
        hits.clear();
        return false;
    }
    return true;
}
// 같은 문서·같은 검색 조건에서는 전체 텍스트 추출과 정규식/일반 검색을 반복하지 않는다.
// 문서 내용 변경은 SetRichText 및 결과 칸 EN_CHANGE에서 g_rpTextSerial로 감지한다.
static wstring g_rpCacheHay, g_rpCacheFind, g_rpCacheRepl, g_rpCacheError;
static std::vector<RpHit> g_rpCacheHits;
static unsigned long long g_rpCacheSerial = 0;
static bool g_rpCacheValid = false, g_rpCacheOk = false;
static bool g_rpCacheCase = false, g_rpCacheWord = false, g_rpCacheRegex = false;
static bool RpSearchCached(wstring& err) {
    const wstring q = RpGet(g_rpFind, g_rpFindX), t = RpGet(g_rpRepl, g_rpReplX);
    const bool cs = Checked(GetDlgItem(g_rpWnd, ID_RP_CASE));
    const bool word = Checked(GetDlgItem(g_rpWnd, ID_RP_WORD));
    const bool rx = Checked(GetDlgItem(g_rpWnd, ID_RP_REGEX));
    const bool searchChanged = !g_rpCacheValid || g_rpCacheSerial != g_rpTextSerial ||
        g_rpCacheFind != q || g_rpCacheCase != cs ||
        g_rpCacheWord != word || g_rpCacheRegex != rx;
    if (searchChanged) {
        g_rpCacheHay = RpHay();
        g_rpCacheOk = RpSearch(g_rpCacheHay, g_rpCacheHits, g_rpCacheError);
        g_rpCacheFind = q; g_rpCacheRepl = t;
        g_rpCacheCase = cs; g_rpCacheWord = word; g_rpCacheRegex = rx;
        g_rpCacheSerial = g_rpTextSerial;
        g_rpCacheValid = true;
    } else if (g_rpCacheRepl != t) {
        // Literal search positions do not depend on the replacement text.
        // Keep original invisible characters inside every match, as before.
        if (!rx && g_rpCacheOk) {
            for (auto& h : g_rpCacheHits) {
                h.rep = t;
                for (size_t k = h.a; k < h.b; ++k)
                    if (IsInvisibleCh((char32_t)g_rpCacheHay[k])) h.rep += g_rpCacheHay[k];
            }
        } else {
            // Regex replacement can contain capture references ($1 etc.).
            // Re-evaluate those formats without extracting the document again.
            g_rpCacheOk = RpSearch(g_rpCacheHay, g_rpCacheHits, g_rpCacheError);
        }
        g_rpCacheRepl = t;
    }
    err = g_rpCacheError;
    return g_rpCacheOk;
}
static void RpInfo(const wstring& s) { SetWindowTextW(g_rpInfo, s.c_str()); }
// ---------------------------------------------------------------------------
// 찾은 곳 모두 표시: 지금 고른 곳은 파란 바탕, 나머지는 노란 바탕 (어두운 배색은 어두운 황토·진한 파랑)
// 글 전체를 칠하지 않고 화면에 보이는 부분(앞뒤 한 화면 여유)만 칠함 — 큰 글에서 찾은 곳이 수만 곳이어도
// 걸리는 시간이 같음. 찾기 창이 열려 있는 동안 짧은 간격으로 보이는 범위·글·찾은 결과·고른 곳·배색을 확인해
// 달라졌을 때만 다시 칠함 (창 크기·전체 화면·글자 크기·줄 간격·쪽 넘김·휠·목차 이동 모두 같은 방식).
// 색은 화면 표시일 뿐: 저장하는 글에는 들어가지 않고, 되돌리기(Ctrl+Z) 기록에도 남기지 않음.
static long g_rpMarkLo = -1, g_rpMarkHi = -1;   // 지금 칠해 둔 범위 (이 안의 바탕색만 바꿈)
static unsigned long long g_rpMarkSerial = 0;
static size_t g_rpMarkCount = 0;
static LONG g_rpMarkSelA = -1, g_rpMarkSelB = -1;
static COLORREF g_rpMarkCur = 0, g_rpMarkOther = 0;
static bool g_rpSelHidden = false;
static HWND g_rpMarkWnd = nullptr;   // 칠한 글 칸 (모드를 바꾸며 글 칸이 새로 만들어지면 지난 상태는 버림)
static void RpMarkFollowWnd() {
    if (g_rpMarkWnd == g_dst) return;
    g_rpMarkWnd = g_dst; g_rpMarkLo = g_rpMarkHi = -1; g_rpSelHidden = false; g_rpMarkCount = 0; g_rpMarkSelA = g_rpMarkSelB = -1;
}
static const size_t kRpMarkMax = 2000;  // 한 번에 칠하는 곳 수의 상한 (한 글자 찾기처럼 화면에 아주 많을 때)
static void RpMarkColors(COLORREF& other, COLORREF& cur) {
    COLORREF fg, bg;
    if (g_reading) ViewColors(fg, bg); else { fg = col::editText; bg = col::editBg; }
    double lum = (0.299 * GetRValue(bg) + 0.587 * GetGValue(bg) + 0.114 * GetBValue(bg)) / 255.0;
    if (lum < 0.45) { other = RGB(102, 82, 22); cur = RGB(30, 80, 200); }   // 어두운 배색: 글자색은 그대로 두고 읽히는 어두운 색
    else { other = RGB(255, 232, 120); cur = RGB(110, 170, 255); }        // 밝은 배색: 노랑 · 파랑
}
static bool RpSetBack(ITextDocument* doc, long a, long b, long color) {
    ITextRange* r = nullptr;
    if (FAILED(doc->Range(a, b, &r)) || !r) return false;
    bool ok = false;
    ITextFont* f = nullptr;
    if (SUCCEEDED(r->GetFont(&f)) && f) { ok = SUCCEEDED(f->SetBackColor(color)); f->Release(); }
    r->Release();
    return ok;
}
static void RpShowSel(bool show) {  // 지금 고른 곳이 찾은 곳이면 선택 표시 대신 파란 바탕이 보이게
    if (g_rpSelHidden == !show || !g_dst) return;
    SendMessageW(g_dst, EM_HIDESELECTION, show ? FALSE : TRUE, 0);
    g_rpSelHidden = !show;
}
// 칠해 둔 범위를 원래 바탕(자동)으로. 글에 다른 바탕색은 없으므로 범위가 조금 어긋나도(글을 고친 뒤) 해가 없음
static void RpMarkPaint(long lo, long hi, const std::vector<RpHit>* hits, LONG selA, LONG selB, COLORREF other, COLORREF cur) {
    ITextDocument* doc = TomDocOf(g_dst);
    if (!doc) return;
    bool ol = g_loading, os = g_syncing;
    g_loading = true; g_syncing = true;  // 바뀜·선택 알림 무시 (글은 그대로)
    {
        RoUnlock unlock(g_dst);
        doc->Undo(tomSuspend, nullptr);
        if (g_rpMarkLo >= 0) RpSetBack(doc, g_rpMarkLo, g_rpMarkHi, tomAutoColor);
        g_rpMarkLo = g_rpMarkHi = -1;
        if (hits) {
            size_t i = RpHitFrom(*hits, (size_t)lo), n = 0;
            // 범위 앞에서 시작해 범위 안으로 들어오는 곳
            if (i > 0 && (*hits)[i - 1].b > (size_t)lo) --i;
            for (; i < hits->size() && (*hits)[i].a < (size_t)hi && n < kRpMarkMax; ++i, ++n) {
                const RpHit& h = (*hits)[i];
                bool now = (LONG)h.a == selA && (LONG)h.b == selB;
                RpSetBack(doc, (long)h.a, (long)h.b, (long)(now ? cur : other));
            }
            g_rpMarkLo = lo; g_rpMarkHi = hi;
        }
        doc->Undo(tomResume, nullptr);
    }
    doc->Release();
    g_loading = ol; g_syncing = os;
}
static void RpMarkClear() {
    RpMarkFollowWnd();
    if (g_rpMarkLo >= 0) RpMarkPaint(0, 0, nullptr, -1, -1, 0, 0);
    RpShowSel(true);
    g_rpMarkCount = 0; g_rpMarkSelA = g_rpMarkSelB = -1;
}
static void RpMarkTick() {
    if (!g_rpWnd || !g_dst) return;
    RpMarkFollowWnd();
    // 글이 바뀌었으면(읽기·정리 모드 전환, 다시 정리, 직접 고침) 잠깐(0.3초) 그대로일 때 같은 조건으로 다시 찾아 표시
    static unsigned long long staleSerial = 0;
    static DWORD staleSince = 0;
    if (!g_busy && g_rpCacheValid && g_rpCacheSerial != g_rpTextSerial && !g_text.empty() && !RpGet(g_rpFind, g_rpFindX).empty()) {
        if (staleSerial != g_rpTextSerial) { staleSerial = g_rpTextSerial; staleSince = GetTickCount(); }
        else if (GetTickCount() - staleSince >= 300) { wstring err; RpSearchCached(err); }
    }
    const bool valid = !g_busy && g_rpCacheValid && g_rpCacheOk && g_rpCacheSerial == g_rpTextSerial && !g_rpCacheHits.empty();
    if (!valid) { if (g_rpMarkLo >= 0 || g_rpSelHidden) RpMarkClear(); return; }
    RECT rc; GetClientRect(g_dst, &rc);
    POINTL p0 = {0, 0}, p1 = {rc.right > 1 ? rc.right - 1 : 0, rc.bottom > 1 ? rc.bottom - 1 : 0};
    long first = (long)SendMessageW(g_dst, EM_CHARFROMPOS, 0, (LPARAM)&p0);
    long last = (long)SendMessageW(g_dst, EM_CHARFROMPOS, 0, (LPARAM)&p1);
    if (last < first) std::swap(first, last);
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    COLORREF other, cur; RpMarkColors(other, cur);
    const auto& hits = g_rpCacheHits;
    // 고른 곳이 찾은 곳인지
    size_t k = RpHitFrom(hits, (size_t)std::max(0L, cr.cpMin));
    bool selIsHit = k < hits.size() && (LONG)hits[k].a == cr.cpMin && (LONG)hits[k].b == cr.cpMax;
    const bool same = g_rpMarkLo >= 0 && first >= g_rpMarkLo && last <= g_rpMarkHi && g_rpMarkSerial == g_rpCacheSerial &&
                      g_rpMarkCount == hits.size() && g_rpMarkSelA == cr.cpMin && g_rpMarkSelB == cr.cpMax &&
                      g_rpMarkCur == cur && g_rpMarkOther == other;
    if (!same) {
        long span = std::max(800L, last - first);
        long lo = std::max(0L, first - span), hi = last + span;
        RpMarkPaint(lo, hi, &hits, cr.cpMin, cr.cpMax, other, cur);
        g_rpMarkSerial = g_rpCacheSerial; g_rpMarkCount = hits.size();
        g_rpMarkSelA = cr.cpMin; g_rpMarkSelB = cr.cpMax; g_rpMarkCur = cur; g_rpMarkOther = other;
    }
    RpShowSel(!selIsHit);
}
// 바꿀 수 있나: 정리 모드는 결과 칸 전체가 보일 때, 읽기 모드는 늘 (글 전체가 보이므로)
static bool RpEditable() { return !g_busy && !g_text.empty() && (g_reading || !g_preview); }
// 큰 글을 읽기 모드에서 바꾼 뒤: 저장·정리 모드 표시가 쓰는 정리 결과에도 반영
static void RpCommit() {
    if (!(g_reading && g_preview)) return;
    wstring w = GetRichText(g_dst);
    g_out = NormalizeNL(WToU32(w.data(), w.size()));
}
// 찾은 곳이 찾기·바꾸기 창에 가려지면 창을 글 칸의 위·아래 반대쪽으로 옮김
static void RpKeepVisible() {
    if (!g_rpWnd) return;
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    POINTL pl = {0, 0};
    SendMessageW(g_dst, EM_POSFROMCHAR, (WPARAM)&pl, cr.cpMin);
    POINT pt = {pl.x, pl.y};
    ClientToScreen(g_dst, &pt);
    RECT wr, dr; GetWindowRect(g_rpWnd, &wr); GetWindowRect(g_dst, &dr);
    int lh = S(40);
    if (pt.y + lh < wr.top || pt.y > wr.bottom || pt.x < wr.left - S(20) || pt.x > wr.right) return;
    int h = wr.bottom - wr.top;
    int top = (wr.top - dr.top < (dr.bottom - dr.top) / 3) ? dr.bottom - h - S(8) : dr.top + S(8);
    if (top < dr.top) top = dr.top;
    SetWindowPos(g_rpWnd, nullptr, wr.left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static void RpRemember(HWND combo) {  // 찾은 말 기록 (이번에 켠 동안)
    wchar_t buf[1024]; GetWindowTextW(combo, buf, 1024);
    if (!buf[0]) return;
    int at = (int)SendMessageW(combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)buf);
    if (at == 0) return;
    if (at > 0) SendMessageW(combo, CB_DELETESTRING, at, 0);
    SendMessageW(combo, CB_INSERTSTRING, 0, (LPARAM)buf);
    SetWindowTextW(combo, buf);
    while (SendMessageW(combo, CB_GETCOUNT, 0, 0) > 15) SendMessageW(combo, CB_DELETESTRING, 15, 0);
}
// 찾을 말을 적는 동안 바로 찾아 보여 줌 (지금 고른 곳부터)
static void RpFindIncr() {
    wstring err;
    if (RpGet(g_rpFind, g_rpFindX).empty()) { RpInfo(L""); return; }
    if (!RpSearchCached(err)) { RpInfo(err); return; }
    const auto& hits = g_rpCacheHits;
    if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return; }
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    const size_t idx = RpHitFrom(hits, (size_t)std::max(0L, cr.cpMin));
    int pick = idx == hits.size() ? 0 : (int)idx;
    SendMessageW(g_dst, EM_SETSEL, hits[pick].a, hits[pick].b);
    SendMessageW(g_dst, EM_SCROLLCARET, 0, 0);
    RpKeepVisible();
    RpMarkTick();
    RpInfo(std::to_wstring(pick + 1) + L" / " + std::to_wstring(hits.size()) + L"곳");
}
static bool RpFindNext(bool quiet = false) {
    wstring err;
    if (!RpSearchCached(err)) { RpInfo(err); return false; }
    const auto& hits = g_rpCacheHits;
    if (!g_rpExpanded) RpRemember(g_rpFind);
    if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return false; }
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    bool up = Checked(GetDlgItem(g_rpWnd, ID_RP_UP));
    int pick = -1;
    bool wrapped = false;
    if (!up) {
        const size_t idx = RpHitFrom(hits, (size_t)std::max(0L, cr.cpMax));
        if (idx == hits.size()) { pick = 0; wrapped = true; }
        else pick = (int)idx;
    } else {
        const size_t idx = RpHitFrom(hits, (size_t)std::max(0L, cr.cpMin));
        if (idx == 0) { pick = (int)hits.size() - 1; wrapped = true; }
        else pick = (int)idx - 1;
    }
    const RpHit& hh = hits[pick];
    SendMessageW(g_dst, EM_SETSEL, hh.a, hh.b);
    SendMessageW(g_dst, EM_SCROLLCARET, 0, 0);
    RpKeepVisible();
    RpMarkTick();
    if (!quiet || wrapped)
        RpInfo(std::to_wstring(pick + 1) + L" / " + std::to_wstring(hits.size()) + L"곳" +
               (wrapped ? (up ? L"   (처음까지 찾아서 끝에서부터 다시 찾음)" : L"   (끝까지 찾아서 처음부터 다시 찾음)") : L""));
    return true;
}
static void RpReplaceOne() {
    if (!RpEditable()) { RpInfo(L"전체 글을 바꾸려면 읽기 모드에서 실행해야 합니다."); return; }
    wstring err;
    if (!RpSearchCached(err)) { RpInfo(err); return; }
    const auto& hits = g_rpCacheHits;
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    const size_t idx = RpHitFrom(hits, (size_t)std::max(0L, cr.cpMin));
    if (idx < hits.size()) {
        const RpHit& hh = hits[idx];
        if ((LONG)hh.a == cr.cpMin && (LONG)hh.b == cr.cpMax) {  // 지금 고른 곳이 찾은 말이면 바꿈
            RpMarkClear();  // 바꾼 글이 찾은 곳의 바탕색을 이어받지 않게
            bool ro = (GetWindowLongW(g_dst, GWL_STYLE) & ES_READONLY) != 0;  // 읽기 모드 칸은 잠깐 풀었다가 다시 잠금
            if (ro) SendMessageW(g_dst, EM_SETREADONLY, FALSE, 0);
            const wstring replacement = hh.rep;  // EN_CHANGE 중 검색 캐시가 갱신되어도 유효한 복사본
            SendMessageW(g_dst, EM_REPLACESEL, TRUE, (LPARAM)replacement.c_str());
            if (ro) SendMessageW(g_dst, EM_SETREADONLY, TRUE, 0);
            g_dstDirty = true;
            RpCommit();
            RpFindNext(true);
            // 다음 찾기에서 이미 갱신된 검색 결과를 그대로 재사용한다.
            wstring e2;
            if (RpSearchCached(e2))
                RpInfo(L"1곳을 바꿨습니다.  남은 곳: " + std::to_wstring(g_rpCacheHits.size()));
            else RpInfo(e2);
            return;
        }
    }
    RpFindNext();
}
static void RpRememberBoth() { if (!g_rpExpanded) { RpRemember(g_rpFind); RpRemember(g_rpRepl); } }
static void RpReplaceAll() {
    if (!RpEditable()) { RpInfo(L"전체 글을 바꾸려면 읽기 모드에서 실행해야 합니다."); return; }
    wstring err;
    if (!RpSearchCached(err)) { RpInfo(err); return; }
    const auto& hay = g_rpCacheHay;
    const auto& hits = g_rpCacheHits;
    if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return; }
    RpMarkClear();
    wstring before = GetRichText(g_dst), out;
    out.reserve(hay.size());
    size_t at = 0;
    for (auto& hh : hits) { out.append(hay, at, hh.a - at); out += hh.rep; at = hh.b; }
    out.append(hay, at, wstring::npos);
    wstring crlf;  // 칸에 넣을 글: 줄 끝 \r\n
    crlf.reserve(out.size() + out.size() / 20);
    for (wchar_t c : out) { if (c == L'\r') crlf += L"\r\n"; else crlf += c; }
    long top = TopChar(g_dst);
    SetRichText(g_dst, crlf);
    ScrollToChar(g_dst, top);
    g_dstDirty = true;
    RpCommit();
    g_rpUndoBefore = before;
    g_rpUndoAfter = GetRichText(g_dst);
    RpInfo(std::to_wstring(hits.size()) + L"곳을 바꿨습니다" + (hits.size() >= 200000 ? L" (한 번에 20만 곳까지, 다시 누르면 이어서 바꿈)" : L"") + L".  되돌리려면 결과 칸에서 Ctrl+Z");
    SetStatus(std::to_wstring(hits.size()) + L"곳을 바꿨습니다. 저장해야 파일에 남습니다.");
}
// 결과 칸 Ctrl+Z: 바로 앞에 한 "모두 바꾸기" 되돌리기
static bool RpUndoAll() {
    if (g_rpUndoAfter.empty()) return false;
    if (GetRichText(g_dst) != g_rpUndoAfter) { g_rpUndoAfter.clear(); g_rpUndoBefore.clear(); return false; }
    RpMarkClear();
    long top = TopChar(g_dst);
    wstring b = g_rpUndoBefore;
    g_rpUndoAfter.clear(); g_rpUndoBefore.clear();
    SetRichText(g_dst, b);
    ScrollToChar(g_dst, top);
    g_dstDirty = true;
    RpCommit();
    SetStatus(L"모두 바꾸기를 되돌렸습니다.");
    if (g_rpWnd) RpInfo(L"모두 바꾸기를 되돌렸습니다.");
    return true;
}
static void RpAddRule() {
    wstring f = RpGet(g_rpFind, g_rpFindX), t = RpGet(g_rpRepl, g_rpReplX);
    if (f.empty()) { RpInfo(L"찾을 말을 적어 주세요."); return; }
    if (f.find(L'\r') != wstring::npos || t.find(L'\r') != wstring::npos) { RpInfo(L"여러 줄로 된 말은 규칙으로 저장할 수 없습니다."); return; }
    bool rx = Checked(GetDlgItem(g_rpWnd, ID_RP_REGEX));
    RuleItem r; r.kind = rx ? 2 : 0; r.f = f; r.t = t; r.on = true;
    if (!RuleStorable(r)) { RpInfo(L"이 말은 규칙으로 저장할 수 없습니다. (앞뒤 공백, #로 시작하는 말, => 가 든 말)"); return; }
    if (rx) { try { std::wregex re(f, std::regex::ECMAScript); } catch (...) { RpInfo(L"정규식 문법이 맞지 않습니다."); return; } }
    if (g_dstDirty && !g_text.empty() &&
        MessageBoxW(g_rpWnd, L"규칙으로 저장하면 글을 다시 정리합니다.\n결과 칸에서 직접 고친 내용은 없어집니다. 계속할까요?", L"규칙으로 저장", MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
        return;
    std::vector<wstring> cm;
    std::vector<RuleItem> v = RulesFromText(LoadRulesText(), &cm);
    for (auto& x : v) if (x.kind == r.kind && x.f == r.f && x.t == r.t) { RpInfo(L"이미 목록에 있는 규칙입니다."); return; }
    v.push_back(r);
    if (!SaveRuleItems(v, cm, g_rpWnd)) return;
    RpInfo(L"규칙으로 저장했습니다. 이 글과 앞으로 정리하는 모든 글에 자동으로 적용됩니다.");
    OptionChanged();
}
// 찾기·바꾸기 창이 놓일 수 있는 화면 영역 (창이 있는 모니터 기준, 전체 화면이면 모니터 전체)
static RECT RpScreenArea() {
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    return g_fullscreen ? mi.rcMonitor : mi.rcWork;
}
// 찾기·바꾸기 창을 프로그램 창(전체 화면이면 화면) 가운데에 놓음
static void RpPlaceCenter(HWND w) {
    if (!w || !g_hwnd) return;
    RECT wr; GetWindowRect(w, &wr);
    RECT mr; GetClientRect(g_hwnd, &mr);
    MapWindowPoints(g_hwnd, nullptr, (POINT*)&mr, 2);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    int x = mr.left + (mr.right - mr.left - ww) / 2, y = mr.top + (mr.bottom - mr.top - wh) / 2;
    RECT wa = RpScreenArea();
    if (x + ww > wa.right) x = wa.right - ww;
    if (x < wa.left) x = wa.left;
    if (y + wh > wa.bottom) y = wa.bottom - wh;
    if (y < wa.top) y = wa.top;
    SetWindowPos(w, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static RECT g_rpOptBox;  // 옵션 묶음 테두리
static void RpLayout(HWND h) {
    RECT rc; GetClientRect(h, &rc);
    int W = rc.right, p = S(18), lw = S(58), bw = S(132), bh = S(30), gap = S(8);
    int bx = W - p - bw, fx = p + lw, fw = bx - S(16) - fx;
    MoveWindow(g_rpFind, fx, S(18), fw, S(300), TRUE);
    MoveWindow(g_rpRepl, fx, S(18) + bh + gap, fw, S(300), TRUE);
    RECT cr; GetWindowRect(g_rpFind, &cr);
    int ch = cr.bottom - cr.top;  // 선택목록 실제 높이에 맞춰 이름표를 가운데로
    MoveWindow(GetDlgItem(h, ID_RP_L1), p, S(18) + (bh - ch) / 2 + (ch - S(20)) / 2, lw, S(20), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_L2), p, S(18) + bh + gap + (bh - ch) / 2 + (ch - S(20)) / 2, lw, S(20), TRUE);
    MoveWindow(g_rpFind, fx, S(18) + (bh - ch) / 2, fw, S(300), TRUE);
    MoveWindow(g_rpRepl, fx, S(18) + bh + gap + (bh - ch) / 2, fw, S(300), TRUE);
    int oy = S(18) + 2 * (bh + gap) + S(4), ow = bx - S(16) - p, cw = (ow - S(24)) / 2;
    g_rpOptBox = {p, oy, p + ow, oy + S(66)};
    MoveWindow(GetDlgItem(h, ID_RP_CASE), p + S(12), oy + S(8), cw, S(24), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_UP), p + S(12) + cw, oy + S(8), cw, S(24), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_WORD), p + S(12), oy + S(36), cw, S(24), TRUE);
    MoveWindow(GetDlgItem(h, ID_RP_REGEX), p + S(12) + cw, oy + S(36), cw, S(24), TRUE);
    MoveWindow(g_rpInfo, p + S(2), oy + S(72), ow, S(20), TRUE);
    int ids[] = {ID_RP_NEXT, ID_RP_REPLACE, ID_RP_ALL, ID_RP_COUNT, ID_RP_EXPAND};
    for (int i = 0; i < 5; ++i) MoveWindow(GetDlgItem(h, ids[i]), bx, S(18) + i * (bh + gap), bw, bh, TRUE);
    int ey = S(18) + 5 * (bh + gap) + S(6);
    if (g_rpExpanded) {
        int eh = (rc.bottom - ey - S(16) - S(48)) / 2;
        MoveWindow(GetDlgItem(h, ID_RP_L3), p, ey, W - p * 2, S(22), TRUE);
        MoveWindow(g_rpFindX, p, ey + S(24), W - p * 2, eh, TRUE);
        MoveWindow(GetDlgItem(h, ID_RP_L4), p, ey + S(24) + eh + S(4), W - p * 2, S(22), TRUE);
        MoveWindow(g_rpReplX, p, ey + S(24) + eh + S(28), W - p * 2, eh, TRUE);
    }
    for (int id : {ID_RP_L3, ID_RP_L4}) ShowWindow(GetDlgItem(h, id), g_rpExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_rpFindX, g_rpExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_rpReplX, g_rpExpanded ? SW_SHOW : SW_HIDE);
    EnableWindow(g_rpFind, !g_rpExpanded);
    EnableWindow(g_rpRepl, !g_rpExpanded);
}
static int RpClientH(bool expanded) { return S(18) + 5 * (S(30) + S(8)) + S(10) + (expanded ? S(300) : 0); }
static void RpSetExpanded(HWND h, bool on) {
    if (on == g_rpExpanded) return;
    // 한 줄 칸 ↔ 여러 줄 칸 내용 옮기기
    if (on) {
        wchar_t buf[1024];
        GetWindowTextW(g_rpFind, buf, 1024); SetWindowTextW(g_rpFindX, buf);
        GetWindowTextW(g_rpRepl, buf, 1024); SetWindowTextW(g_rpReplX, buf);
    }
    g_rpExpanded = on;
    SetWindowTextW(GetDlgItem(h, ID_RP_EXPAND), on ? L"축소" : L"확장");
    RECT wr; GetWindowRect(h, &wr);
    RECT cr = {0, 0, 10, RpClientH(on)};
    AdjustWindowRectEx(&cr, (DWORD)GetWindowLongW(h, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(h, GWL_EXSTYLE));
    int nh = cr.bottom - cr.top, ny = wr.top;
    RECT wa = RpScreenArea();
    if (ny + nh > wa.bottom) ny = std::max<int>(wa.top, wa.bottom - nh);  // 화면 아래로 넘치지 않게
    SetWindowPos(h, nullptr, wr.left, ny, wr.right - wr.left, nh, SWP_NOZORDER);
    RpLayout(h);
    SetFocus(on ? g_rpFindX : g_rpFind);
    if (on) {  // 옮겨 온 글은 골라 둠: 바로 Enter를 치면 그 글 대신 줄바꿈이 들어감
        SendMessageW(g_rpFindX, EM_SETSEL, 0, -1);
        SendMessageW(g_rpReplX, EM_SETSEL, 0, -1);
    }
}
static void RpUpdateState() {
    if (!g_rpWnd) return;
    // 정리 미리보기에서도 클릭을 허용하고, 실제 실행 전에 전환 동의를 받는다.
    const bool available = !g_busy && !g_text.empty();
    for (int id : {ID_RP_REPLACE, ID_RP_ALL})
        EnableWindow(GetDlgItem(g_rpWnd, id), available);
    if (available && g_preview && !g_reading && GetWindowTextLengthW(g_rpInfo) == 0)
        RpInfo(L"큰 파일: 바꾸기 시 읽기 모드 전환을 확인합니다.");
}
// 큰 파일 정리 미리보기는 전체 글이 아니므로 확인받은 뒤 전체 글에서 실행한다.
// 찾기 창을 재생성하지 않으므로 찾기/바꾸기 문자열, 옵션, 확장 입력이 유지된다.
static void RpRunReplaceAction(bool all) {
    if (g_preview && !g_reading && !g_busy && !g_text.empty()) {
        if (RpGet(g_rpFind, g_rpFindX).empty()) {
            RpInfo(L"찾을 말을 적어 주세요.");
            return;
        }
        const wstring action = all ? L"모두 바꾸기" : L"바꾸기";
        const wstring prompt =
            L"정리 모드에서는 큰 파일의 앞부분만 화면에 보여 줍니다.\n\n"
            L"현재 찾을 말·바꿀 말·검색 옵션을 그대로 유지한 채 읽기 모드로 전환해\n"
            L"전체 글에서 [" + action + L"] 작업을 실행할까요?\n\n"
            L"[예] 읽기 모드 전환 후 " + action + L" 실행\n"
            L"[아니요] 모드·검색창·본문 유지";
        if (MessageBoxW(g_rpWnd, prompt.c_str(), L"전체 글 바꾸기 · 읽기 모드 전환", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            return;
        CHARRANGE oldSel = {};
        SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&oldSel);
        KillTimer(g_rpWnd, 7);
        SetReading(true); // 전체 글 로드 및 검색 캐시 자동 무효화
        if (!g_reading || !RpEditable()) {
            RpInfo(L"읽기 모드에서 전체 글을 준비하지 못했습니다. 다시 시도해 주세요.");
            return;
        }
        RpUpdateState();
        if (all) {
            RpRememberBoth();
            RpReplaceAll();
        } else {
            wstring err;
            if (!RpSearchCached(err)) { RpInfo(err); return; }
            const auto& hits = g_rpCacheHits;
            if (hits.empty()) { RpInfo(L"찾는 말이 없습니다."); return; }
            const size_t idx = RpHitFrom(hits, (size_t)std::max(0L, oldSel.cpMin));
            const size_t pick = idx == hits.size() ? 0 : idx;
            SendMessageW(g_dst, EM_SETSEL, (WPARAM)hits[pick].a, (LPARAM)hits[pick].b);
            SendMessageW(g_dst, EM_SCROLLCARET, 0, 0);
            RpRememberBoth();
            RpReplaceOne();
        }
        if (IsWindow(g_rpWnd)) SetFocus(g_rpExpanded ? g_rpFindX : g_rpFind);
        return;
    }
    if (!RpEditable()) {
        RpInfo(g_busy ? L"글을 정리하는 중입니다. 완료 후 다시 시도하세요." : L"먼저 파일을 열거나 글을 붙여넣으세요.");
        return;
    }
    RpRememberBoth();
    if (all) RpReplaceAll();
    else RpReplaceOne();
}
static LRESULT CALLBACK RpProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_rpWnd = h;
        auto btn = [&](int id, const wchar_t* t) {
            HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SetWindowSubclass(b, BtnProc, 0, 0);
            return b;
        };
        auto lab = [&](int id, const wchar_t* t, HFONT f) {
            HWND s = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(s, WM_SETFONT, (WPARAM)f, TRUE);
            return s;
        };
        auto combo = [&](int id) {
            HWND c = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL,
                                     0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            return c;
        };
        auto multi = [&](int id) {
            HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                     0, 0, 10, 10, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(e, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            return e;
        };
        lab(ID_RP_L1, L"찾을 말", g_uiFont);
        lab(ID_RP_L2, L"바꿀 말", g_uiFont);
        g_rpFind = combo(ID_RP_FIND);
        g_rpRepl = combo(ID_RP_REPL);
        btn(ID_RP_CASE, L"대소문자 구분");
        btn(ID_RP_WORD, L"온전한 단어만");
        btn(ID_RP_UP, L"위쪽으로 찾기");
        btn(ID_RP_REGEX, L"정규식");
        g_rpInfo = lab(ID_RP_INFO, L"", g_smallFont);
        btn(ID_RP_NEXT, L"찾기");
        btn(ID_RP_REPLACE, L"바꾸기");
        btn(ID_RP_ALL, L"모두 바꾸기");
        btn(ID_RP_COUNT, L"횟수");
        btn(ID_RP_EXPAND, L"확장");
        lab(ID_RP_L3, L"찾을 말 (여러 줄)", g_uiFont);
        g_rpFindX = multi(ID_RP_FINDX);
        lab(ID_RP_L4, L"바꿀 말 (여러 줄)", g_uiFont);
        g_rpReplX = multi(ID_RP_REPLX);
        RpLoadState(h);
        for (HWND c : {g_rpFind, g_rpRepl}) ApplyCtlTheme(c, L"DarkMode_CFD");
        for (HWND c : {g_rpFindX, g_rpReplX}) ApplyScrollTheme(c);
        RpLayout(h);
        SetTimer(h, 8, 60, nullptr);  // 찾은 곳 표시: 보이는 범위·글·고른 곳이 바뀌었는지 확인
        return 0;
    }
    case WM_SIZE: RpLayout(h); InvalidateRect(h, nullptr, TRUE); return 0;
    case WM_ERASEBKGND: {  // 바탕 + 창 테두리 + 옵션 묶음 테두리
        HDC dc = (HDC)wp;
        RECT rc; GetClientRect(h, &rc);
        FillRect2(dc, rc, col::panel);
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillRound(g, g_rpOptBox, S(6), col::panel, col::border);
        g.SetSmoothingMode(SmoothingModeNone);
        Pen edge(GC(col::boxBorder), 1.0f);
        g.DrawRectangle(&edge, 0, 0, rc.right - 1, rc.bottom - 1);
        return 1;
    }
    case WM_DRAWITEM: DrawButton((const DRAWITEMSTRUCT*)lp); return TRUE;
    case WM_CTLCOLORSTATIC: {
        SetBkColor((HDC)wp, col::panel);
        SetTextColor((HDC)wp, GetDlgCtrlID((HWND)lp) == ID_RP_INFO ? col::okText : col::text);
        return (LRESULT)g_brPanel;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        SetBkColor((HDC)wp, col::field);
        SetTextColor((HDC)wp, col::text);
        return (LRESULT)g_brField;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE) RpUpdateState();
        break;
    case WM_TIMER:
        if (wp == 7) { KillTimer(h, 7); if (!g_text.empty()) RpFindIncr(); return 0; }
        if (wp == 8) { RpMarkTick(); return 0; }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDCANCEL) { DestroyWindow(h); return 0; }
        if ((id == ID_RP_FIND && (HIWORD(wp) == CBN_EDITCHANGE || HIWORD(wp) == CBN_SELCHANGE)) || (id == ID_RP_FINDX && HIWORD(wp) == EN_CHANGE)) {
            SetTimer(h, 7, 300, nullptr);  // 적는 동안 바로 찾기 (잠깐 멈추면)
            return 0;
        }
        if (id == IDOK) id = ID_RP_NEXT;  // Enter: 찾기
        else if (HIWORD(wp) != BN_CLICKED) break;
        if (g_text.empty() && id != ID_RP_LIST && id != ID_RP_EXPAND && id != ID_RP_CASE && id != ID_RP_WORD && id != ID_RP_UP && id != ID_RP_REGEX && id != ID_RP_RULE) {
            RpInfo(L"먼저 파일을 열거나 글을 붙여넣으세요.");
            return 0;
        }
        switch (id) {
        case ID_RP_CASE: case ID_RP_WORD: case ID_RP_UP: case ID_RP_REGEX: {
            HWND b = GetDlgItem(h, id); SetToggle(b, !Checked(b)); return 0;
        }
        case ID_RP_NEXT: RpFindNext(); return 0;
        case ID_RP_REPLACE: RpRunReplaceAction(false); return 0;
        case ID_RP_ALL: RpRunReplaceAction(true); return 0;
        case ID_RP_COUNT: {
            wstring err;
            if (!RpSearchCached(err)) RpInfo(err);
            else RpInfo(g_rpCacheHits.empty() ? L"찾는 말이 없습니다." : std::to_wstring(g_rpCacheHits.size()) + L"곳 있습니다." + (g_preview && !g_reading ? L"  (화면에 보이는 앞부분 기준)" : L""));
            return 0;
        }
        case ID_RP_RULE: RpAddRule(); return 0;
        case ID_RP_LIST: {
            wstring pre = g_rpExpanded ? L"" : RpGet(g_rpFind, g_rpFindX);
            OpenRules(pre);
            return 0;
        }
        case ID_RP_EXPAND: RpSetExpanded(h, !g_rpExpanded); return 0;
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        KillTimer(h, 8);
        RpMarkClear();  // 찾은 곳 표시를 모두 지움
        RpSaveState();
        g_rpCacheValid = false;
        wstring().swap(g_rpCacheHay);
        std::vector<RpHit>().swap(g_rpCacheHits);  // 큰 파일 검색 메모리는 창을 닫으면 반환
        g_rpWnd = nullptr; g_rpExpanded = false; SetFocus(g_dst); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static void OpenReplace() {
    if (g_rpWnd) {
        ShowWindow(g_rpWnd, SW_SHOW);
        SetForegroundWindow(g_rpWnd);
    } else {
        static bool reg = false;
        if (!reg) {
            WNDCLASSEXW wc = {sizeof(wc)};
            wc.lpfnWndProc = RpProc;
            wc.hInstance = g_hInst;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.lpszClassName = L"YamoonReplace";
            wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
            RegisterClassExW(&wc);
            reg = true;
        }
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        RECT cr = {0, 0, S(560), RpClientH(false)};
        AdjustWindowRectEx(&cr, style, FALSE, WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW);
        int w = cr.right - cr.left, hh = cr.bottom - cr.top;
        HWND rw = CreateWindowExW(WS_EX_DLGMODALFRAME, L"YamoonReplace", L"찾기·바꾸기", style, 0, 0, w, hh, g_hwnd, nullptr, g_hInst, nullptr);
        if (!rw) return;
        RpPlaceCenter(rw);  // 프로그램 창(전체 화면이면 화면) 가운데
        SetClassLongPtrW(rw, GCLP_HBRBACKGROUND, (LONG_PTR)g_brPanel);
        ApplyTitleBar(rw, true);  // 글 칸과 구분되게 테두리를 강조색으로
        ShowWindow(rw, SW_SHOW);
    }
    // 결과 칸에서 고른 말이 있으면 찾을 말로
    CHARRANGE cr; SendMessageW(g_dst, EM_EXGETSEL, 0, (LPARAM)&cr);
    if (cr.cpMax > cr.cpMin && cr.cpMax - cr.cpMin < 200) {
        wstring hay = RpHay();
        if ((size_t)cr.cpMax <= hay.size()) {
            wstring s = hay.substr(cr.cpMin, cr.cpMax - cr.cpMin);
            if (s.find(L'\r') == wstring::npos) SetWindowTextW(g_rpFind, s.c_str());
        }
    }
    RpUpdateState();
    SetFocus(g_rpExpanded ? g_rpFindX : g_rpFind);
    SendMessageW(g_rpFind, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
}

