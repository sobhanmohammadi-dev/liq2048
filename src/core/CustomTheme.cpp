#include "core/CustomTheme.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace liq {

namespace {
constexpr size_t kMaxHeader = 16 * 1024;
constexpr size_t kMaxTotal = 24u * 1024u * 1024u;
constexpr size_t kMaxBlob = 8u * 1024u * 1024u;
constexpr size_t kMaxFileName = 40;

void put32(std::vector<unsigned char>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back((unsigned char)((x >> (8 * i)) & 255u));
}
uint32_t get32(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

bool validUtf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = (unsigned char)s[i];
        int n = 0;
        uint32_t cp = 0;
        if (c < 0x80) { if (c < 32 || c == 127) return false; ++i; continue; }
        else if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
        else return false;
        if (i + size_t(n) >= s.size()) return false;
        for (int k = 1; k <= n; ++k) {
            const unsigned char d = (unsigned char)s[i + size_t(k)];
            if ((d & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (d & 0x3F);
        }
        if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += size_t(n) + 1;
    }
    return true;
}

bool validLang(const std::string& l) {
    if (l.size() < 2 || l.size() > 3) return false;
    for (char ch : l)
        if (ch < 'a' || ch > 'z') return false;
    return true;
}

bool sniffAudio(const std::vector<unsigned char>& b) {
    return b.size() >= 12 && (std::memcmp(b.data(), "OggS", 4) == 0 || (std::memcmp(b.data(), "RIFF", 4) == 0 && std::memcmp(b.data() + 8, "WAVE", 4) == 0));
}

const char* const kColorKeys[kThemeColors] = {
    "ct.c.sky1", "ct.c.sky2", "ct.c.sky3", "ct.c.sky4", "ct.c.sky5", "ct.c.star", "ct.c.star2", "ct.c.ink", "ct.c.panel", "ct.c.panelHi",
    "ct.c.panelLo", "ct.c.tank", "ct.c.tankDot", "ct.c.frame", "ct.c.frameHi", "ct.c.frameLo", "ct.c.text", "ct.c.dim", "ct.c.accent",
    "ct.c.accentDark", "ct.c.danger", "ct.c.label", "ct.c.blob1", "ct.c.blob2", "ct.c.blob3", "ct.c.blob4", "ct.c.blob5", "ct.c.blob6",
    "ct.c.blob7", "ct.c.blob8", "ct.c.blob9", "ct.c.blob10", "ct.c.blob11", "ct.c.blob12"};
const char* const kSlotKeys[kAudioSlots] = {"ct.s.menu", "ct.s.calm", "ct.s.intense", "ct.s.drop", "ct.s.splash", "ct.s.merge",
                                            "ct.s.chain", "ct.s.click", "ct.s.swipe", "ct.s.fail", "ct.s.success", "ct.s.unlock"};
const char* const kSlotFiles[kAudioSlots] = {"menu", "calm", "intense", "drop", "splash", "merge", "chain", "click", "swipe", "fail", "success", "unlock"};
const char* const kBuiltinNames[4] = {"Default", "Steel", "Sunset", "Forest"};
}  // namespace

uint32_t crc32(const unsigned char* d, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ d[i]) & 255u] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::string base64Encode(const unsigned char* d, size_t n) {
    static const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = uint32_t(d[i]) << 16 | (i + 1 < n ? uint32_t(d[i + 1]) << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o += a[(v >> 18) & 63];
        o += a[(v >> 12) & 63];
        o += i + 1 < n ? a[(v >> 6) & 63] : '=';
        o += i + 2 < n ? a[v & 63] : '=';
    }
    return o;
}

bool base64Decode(const std::string& s, std::vector<unsigned char>& out) {
    out.clear();
    uint32_t acc = 0;
    int bits = 0, pad = 0;
    for (char ch : s) {
        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') continue;
        int v;
        if (ch >= 'A' && ch <= 'Z') v = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') v = ch - '0' + 52;
        else if (ch == '+') v = 62;
        else if (ch == '/') v = 63;
        else if (ch == '=') { ++pad; continue; }
        else { out.clear(); return false; }
        if (pad) { out.clear(); return false; }
        acc = (acc << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((unsigned char)((acc >> bits) & 255u));
            if (out.size() > kMaxTotal) { out.clear(); return false; }
        }
    }
    return true;
}

CustomTheme::CustomTheme() { fromBuiltin(0, false); }

void CustomTheme::fromBuiltin(int i, bool withName) {
    i = std::clamp(i, 0, kThemeCount - 1);
    theme = themeAt(i);
    theme.key = "theme.custom";
    for (int s = 0; s < kAudioSlots; ++s) {
        pack.slot[s].palette = i;
        pack.slot[s].data.clear();
        fileName[s].clear();
    }
    if (withName) {
        nameEn = kBuiltinNames[i];
        names.clear();
    }
}

bool CustomTheme::hasFiles() const {
    for (const SlotSource& s : pack.slot)
        if (!s.data.empty()) return true;
    return false;
}

std::string CustomTheme::displayName(const std::string& lang) const {
    auto it = names.find(lang);
    if (lang != "en" && it != names.end() && !it->second.empty()) return it->second;
    return nameEn.empty() ? "Custom" : nameEn;
}

std::string CustomTheme::cleanEnglish(const std::string& s) {
    std::string o;
    for (char ch : s)
        if (ch >= 32 && ch <= 126) o += ch;
    while (!o.empty() && o.front() == ' ') o.erase(o.begin());
    if (o.size() > 24) o.resize(24);
    while (!o.empty() && o.back() == ' ') o.pop_back();
    return o;
}

std::string CustomTheme::cleanLocal(const std::string& s) {
    std::string o = s;
    while (o.size() > 48) o.pop_back();
    while (!o.empty() && !validUtf8(o)) o.pop_back();  // cut a half-written character
    while (!o.empty() && o.front() == ' ') o.erase(o.begin());
    while (!o.empty() && o.back() == ' ') o.pop_back();
    return validUtf8(o) ? o : std::string();
}

Color& CustomTheme::colorAt(Theme& t, int i) {
    if (i < 5) return t.sky[i];
    if (i >= 22) return t.blob[std::min(i - 22, 11)];
    Color* c[] = {&t.star, &t.star2, &t.ink, &t.panel, &t.panelHi, &t.panelLo, &t.tank, &t.tankDot, &t.frame,
                  &t.frameHi, &t.frameLo, &t.text, &t.dim, &t.accent, &t.accentDark, &t.danger, &t.label};
    return *c[std::clamp(i - 5, 0, 16)];
}
const char* CustomTheme::colorKey(int i) { return kColorKeys[std::clamp(i, 0, kThemeColors - 1)]; }
const char* CustomTheme::slotKey(int s) { return kSlotKeys[std::clamp(s, 0, kAudioSlots - 1)]; }
const char* CustomTheme::slotFile(int s) { return kSlotFiles[std::clamp(s, 0, kAudioSlots - 1)]; }

std::vector<unsigned char> CustomTheme::serialize() const {
    std::ostringstream h;
    h << "name=" << cleanEnglish(nameEn) << "\n";
    for (const auto& kv : names) {
        const std::string n = cleanLocal(kv.second);
        if (validLang(kv.first) && kv.first != "en" && !n.empty()) h << "name." << kv.first << "=" << n << "\n";
    }
    Theme t = theme;
    for (int i = 0; i < kThemeColors; ++i) {
        const Color c = colorAt(t, i);
        char b[24];
        std::snprintf(b, sizeof b, "c%d=%02x%02x%02x\n", i, c.r, c.g, c.b);
        h << b;
    }
    for (int s = 0; s < kAudioSlots; ++s) {
        const SlotSource& src = pack.slot[s];
        if (src.data.empty()) {
            h << "slot" << s << "=pal:" << std::clamp(src.palette, 0, 3) << "\n";
        } else {
            std::string fn;
            for (char ch : fileName[s])
                if (ch >= 33 && ch <= 126 && ch != '/' && ch != '\\' && fn.size() < kMaxFileName) fn += ch;
            if (fn.empty()) fn = "sound";
            h << "slot" << s << "=file:" << src.data.size() << ":" << fn << "\n";
        }
    }
    const std::string hs = h.str();
    std::vector<unsigned char> o;
    o.insert(o.end(), {'L', 'I', 'Q', 'T', 'H', 'E', 'M', 'E'});
    put32(o, 1);
    put32(o, uint32_t(hs.size()));
    o.insert(o.end(), hs.begin(), hs.end());
    for (const SlotSource& src : pack.slot) o.insert(o.end(), src.data.begin(), src.data.end());
    put32(o, crc32(o.data(), o.size()));
    return o;
}

bool CustomTheme::parse(const unsigned char* d, size_t n, CustomTheme& out) {
    if (!d || n < 20 || n > kMaxTotal + kMaxHeader + 64) return false;
    if (std::memcmp(d, "LIQTHEME", 8) != 0 || get32(d + 8) != 1) return false;
    const size_t hl = get32(d + 12);
    if (hl == 0 || hl > kMaxHeader || 16 + hl + 4 > n) return false;
    if (crc32(d, n - 4) != get32(d + n - 4)) return false;

    std::map<std::string, std::string> kv;
    {
        const std::string hs((const char*)d + 16, hl);
        size_t pos = 0;
        while (pos < hs.size()) {
            size_t e = hs.find('\n', pos);
            if (e == std::string::npos) e = hs.size();
            const std::string line = hs.substr(pos, e - pos);
            pos = e + 1;
            if (line.empty()) continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos || eq == 0) return false;
            if (kv.size() > 128) return false;
            kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    CustomTheme t;
    auto nm = kv.find("name");
    if (nm == kv.end()) return false;
    t.nameEn = cleanEnglish(nm->second);
    if (t.nameEn.empty() || t.nameEn != nm->second) return false;
    for (const auto& e : kv) {
        if (e.first.rfind("name.", 0) != 0) continue;
        const std::string lang = e.first.substr(5);
        if (!validLang(lang) || lang == "en") continue;
        if (e.second.size() > 48 || !validUtf8(e.second)) return false;
        if (!e.second.empty()) t.names[lang] = e.second;
    }
    for (int i = 0; i < kThemeColors; ++i) {
        auto it = kv.find("c" + std::to_string(i));
        if (it == kv.end() || it->second.size() != 6) return false;
        uint32_t v = 0;
        for (char ch : it->second) {
            int x;
            if (ch >= '0' && ch <= '9') x = ch - '0';
            else if (ch >= 'a' && ch <= 'f') x = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') x = ch - 'A' + 10;
            else return false;
            v = v * 16 + uint32_t(x);
        }
        colorAt(t.theme, i) = rgb(v);
    }
    size_t pos = 16 + hl;
    const size_t end = n - 4;
    for (int s = 0; s < kAudioSlots; ++s) {
        auto it = kv.find("slot" + std::to_string(s));
        if (it == kv.end()) return false;
        const std::string& v = it->second;
        if (v.rfind("pal:", 0) == 0) {
            if (v.size() != 5 || v[4] < '0' || v[4] > '3') return false;
            t.pack.slot[s].palette = v[4] - '0';
        } else if (v.rfind("file:", 0) == 0) {
            const size_t c2 = v.find(':', 5);
            if (c2 == std::string::npos || c2 == 5 || c2 - 5 > 8) return false;
            size_t len = 0;
            for (size_t k = 5; k < c2; ++k) {
                if (v[k] < '0' || v[k] > '9') return false;
                len = len * 10 + size_t(v[k] - '0');
            }
            const std::string fn = v.substr(c2 + 1);
            if (len == 0 || len > kMaxBlob || pos + len > end || fn.size() > kMaxFileName) return false;
            for (char ch : fn)
                if (ch < 33 || ch > 126) return false;
            t.pack.slot[s].data.assign(d + pos, d + pos + len);
            if (!sniffAudio(t.pack.slot[s].data)) return false;
            t.fileName[s] = fn;
            pos += len;
        } else {
            return false;
        }
    }
    if (pos != end) return false;  // no stray bytes
    t.theme.key = "theme.custom";
    out = std::move(t);
    return true;
}

std::string CustomTheme::toClipboard() const {
    if (hasFiles()) return {};
    const auto b = serialize();
    return "LIQTHEME:" + base64Encode(b.data(), b.size());
}

bool CustomTheme::fromClipboard(const std::string& text, CustomTheme& out) {
    size_t a = 0;
    while (a < text.size() && (text[a] == ' ' || text[a] == '\n' || text[a] == '\r')) ++a;
    if (text.compare(a, 9, "LIQTHEME:") != 0 || text.size() > 40000) return false;
    std::vector<unsigned char> b;
    return base64Decode(text.substr(a + 9), b) && parse(b.data(), b.size(), out);
}

}  // namespace liq
