#include "gfx/ArabicShaper.hpp"

#include <algorithm>
#include <vector>

namespace liq {

namespace {

struct Forms {
    char32_t base, iso, fin, ini, med;  // 0 = form does not exist
};

// Right-joining letters have no initial/medial form.
constexpr Forms kForms[] = {
    {0x0621, 0xFE80, 0, 0, 0},           {0x0622, 0xFE81, 0xFE82, 0, 0},       {0x0623, 0xFE83, 0xFE84, 0, 0},
    {0x0624, 0xFE85, 0xFE86, 0, 0},      {0x0625, 0xFE87, 0xFE88, 0, 0},       {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},
    {0x0627, 0xFE8D, 0xFE8E, 0, 0},      {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92}, {0x0629, 0xFE93, 0xFE94, 0, 0},
    {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98}, {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C}, {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},
    {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4}, {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8}, {0x062F, 0xFEA9, 0xFEAA, 0, 0},
    {0x0630, 0xFEAB, 0xFEAC, 0, 0},      {0x0631, 0xFEAD, 0xFEAE, 0, 0},       {0x0632, 0xFEAF, 0xFEB0, 0, 0},
    {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4}, {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8}, {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},
    {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0}, {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4}, {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},
    {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC}, {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0}, {0x0640, 0x0640, 0x0640, 0x0640, 0x0640},
    {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4}, {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8}, {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC},
    {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0}, {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4}, {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8},
    {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC}, {0x0648, 0xFEED, 0xFEEE, 0, 0},       {0x0649, 0xFEEF, 0xFEF0, 0, 0},
    {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},
    // Persian / Urdu additions
    {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59}, {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D}, {0x0698, 0xFB8A, 0xFB8B, 0, 0},
    {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91}, {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95}, {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF},
};

const Forms* findForms(char32_t c) {
    for (const auto& f : kForms)
        if (f.base == c) return &f;
    return nullptr;
}

bool isMark(char32_t c) { return (c >= 0x064B && c <= 0x065F) || c == 0x0670; }  // harakat: dropped (not shaped)
bool joinsNext(const Forms* f) { return f && f->ini != 0; }  // dual-joining: connects to the following letter
bool joinsPrev(const Forms* f) { return f && f->fin != 0; }  // connects to the preceding letter

std::u32string decode(const std::string& s) {
    std::u32string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0xFFFD;
        int n = 1;
        if (c < 0x80) cp = c;
        else if ((c >> 5) == 6 && i + 1 < s.size()) { cp = char32_t((c & 0x1F) << 6 | (s[i + 1] & 0x3F)); n = 2; }
        else if ((c >> 4) == 14 && i + 2 < s.size()) { cp = char32_t((c & 0x0F) << 12 | (s[i + 1] & 0x3F) << 6 | (s[i + 2] & 0x3F)); n = 3; }
        else if ((c >> 3) == 30 && i + 3 < s.size()) { cp = char32_t((c & 0x07) << 18 | (s[i + 1] & 0x3F) << 12 | (s[i + 2] & 0x3F) << 6 | (s[i + 3] & 0x3F)); n = 4; }
        out.push_back(cp);
        i += size_t(n);
    }
    return out;
}

void encode(char32_t cp, std::string& out) {
    if (cp < 0x80) out += char(cp);
    else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
    else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
}

enum class Dir { R, L, N };

Dir dirOf(char32_t c) {
    if (c == 0x200C || c == 0x200D) return Dir::R;
    if ((c >= 0x0600 && c <= 0x06FF && !(c >= 0x0660 && c <= 0x0669) && !(c >= 0x06F0 && c <= 0x06F9)) || (c >= 0xFB50 && c <= 0xFEFF))
        return (c == 0x060C || c == 0x061B || c == 0x061F) ? Dir::N : Dir::R;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0x0660 && c <= 0x0669) ||
        (c >= 0x06F0 && c <= 0x06F9) || (c >= 0x00C0 && c <= 0x024F))
        return Dir::L;
    return Dir::N;
}

char32_t mirrored(char32_t c) {
    switch (c) {
        case '(': return ')';
        case ')': return '(';
        case '<': return '>';
        case '>': return '<';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        default: return c;
    }
}

}  // namespace

std::string shapeArabicVisual(const std::string& utf8) {
    const std::u32string in = decode(utf8);
    // ---- 1. shaping (logical order) ----
    std::u32string sh;
    for (size_t i = 0; i < in.size(); ++i) {
        const char32_t c = in[i];
        if (isMark(c) || c == 0x200D) continue;
        const Forms* f = findForms(c);
        if (!f) { sh.push_back(c == 0x200C ? char32_t(0x200C) : c); continue; }
        // Previous / next letters in the same word (marks are skipped; anything else breaks the word).
        const Forms* pf = nullptr;
        for (size_t k = i; k-- > 0;) {
            if (isMark(in[k])) continue;
            pf = findForms(in[k]);
            break;
        }
        const Forms* nf = nullptr;
        size_t ni = i;
        for (size_t k = i + 1; k < in.size(); ++k) {
            if (isMark(in[k])) continue;
            nf = findForms(in[k]);
            ni = k;
            break;
        }
        const bool prevJoins = pf && joinsNext(pf) && joinsPrev(f);
        // lam + alef ligature
        if (c == 0x0644 && nf && (nf->base == 0x0622 || nf->base == 0x0623 || nf->base == 0x0625 || nf->base == 0x0627)) {
            const char32_t iso = nf->base == 0x0622 ? 0xFEF5 : nf->base == 0x0623 ? 0xFEF7 : nf->base == 0x0625 ? 0xFEF9 : 0xFEFB;
            sh.push_back(prevJoins ? iso + 1 : iso);
            i = ni;
            continue;
        }
        const bool nextJoins = joinsNext(f) && nf && joinsPrev(nf);
        char32_t g = f->iso;
        if (prevJoins && nextJoins && f->med) g = f->med;
        else if (prevJoins && f->fin) g = f->fin;
        else if (nextJoins && f->ini) g = f->ini;
        sh.push_back(g);
    }
    // ---- 2. bidi (base direction RTL) ----
    std::vector<Dir> t(sh.size());
    for (size_t i = 0; i < sh.size(); ++i) t[i] = dirOf(sh[i]);
    for (size_t i = 0; i < sh.size();) {  // neutrals between two LTR runs are LTR, otherwise RTL
        if (t[i] != Dir::N) { ++i; continue; }
        size_t j = i;
        while (j < sh.size() && t[j] == Dir::N) ++j;
        const bool l = i > 0 && t[i - 1] == Dir::L, r = j < sh.size() && t[j] == Dir::L;
        for (size_t k = i; k < j; ++k) t[k] = (l && r) ? Dir::L : Dir::R;
        i = j;
    }
    struct Run { size_t a, b; Dir d; };
    std::vector<Run> runs;
    for (size_t i = 0; i < sh.size();) {
        size_t j = i;
        while (j < sh.size() && t[j] == t[i]) ++j;
        runs.push_back({i, j, t[i]});
        i = j;
    }
    std::string out;
    for (size_t r = runs.size(); r-- > 0;) {
        const Run& run = runs[r];
        if (run.d == Dir::L) {
            for (size_t k = run.a; k < run.b; ++k) encode(sh[k], out);
        } else {
            for (size_t k = run.b; k-- > run.a;) {
                if (sh[k] == 0x200C) continue;  // joiner controls are invisible
                encode(mirrored(sh[k]), out);
            }
        }
    }
    return out;
}

}  // namespace liq
