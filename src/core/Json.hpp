#pragma once
// Minimal parser for flat {"key": "value", ...} string tables (localization).
// Only quoted strings are read; \n \t \" \\ escapes supported.
#include <string>
#include <unordered_map>

namespace liq {

inline std::unordered_map<std::string, std::string> parseFlatJson(const std::string& s) {
    std::unordered_map<std::string, std::string> m;
    std::string key, cur;
    bool haveKey = false;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '"') {
            ++i;
            continue;
        }
        ++i;
        cur.clear();
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                ++i;
                cur += s[i] == 'n' ? '\n' : (s[i] == 't' ? '\t' : s[i]);
            } else {
                cur += s[i];
            }
            ++i;
        }
        ++i;
        if (!haveKey) {
            key = cur;
            haveKey = true;
        } else {
            m[key] = cur;
            haveKey = false;
        }
    }
    return m;
}

}  // namespace liq
