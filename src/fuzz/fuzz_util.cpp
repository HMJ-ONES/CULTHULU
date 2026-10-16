#include "fuzz/fuzz_util.h"

#include <cmath>
#include <cstdarg>
#include <cstring>
#include <limits>

namespace fuzz {

namespace {
// Raw fd that failure reports go to (the silencer's saved stderr).
int g_reportFd = 2;
} // namespace

void setReportFd(int fd) { g_reportFd = fd; }

void logf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    // Best effort: never throws, never blocks the harness.
    ::write(g_reportFd, buf, std::strlen(buf));
}

float randomFloat(FuzzRng& rng) {
    const double r = rng.uni01();
    if (r < 0.55)
        return static_cast<float>(rng.range(-1000000, 1000000)) / 100.0f;
    if (r < 0.65)
        return -static_cast<float>(rng.range(0, 1000000));
    if (r < 0.72)
        return std::numeric_limits<float>::infinity();
    if (r < 0.78)
        return -std::numeric_limits<float>::infinity();
    if (r < 0.85)
        return std::numeric_limits<float>::quiet_NaN();
    if (r < 0.93)
        return 1.0e30f * (rng.chance(0.5) ? 1.0f : -1.0f);
    if (r < 0.97)
        // Denormal-range value (below FLT_MIN ~1.18e-38). Written as a
        // literal: this toolchain's <limits> lacks denormal_min().
        return 1.0e-40f * (rng.chance(0.5) ? 1.0f : -1.0f);
    return static_cast<float>(rng.range(-10, 10)); // small sane values
}

std::string randomTag(FuzzRng& rng) {
    static const char* const sane[] = {
        "",           "relic",   "directive_raid", "directive_war",
        "GoToWar/Obeyed", "RaidCity/Refused", "Defend/PartiallyObeyed",
        "chaos_punish",   "refused_directive",    "belief_tension",
        "interruptedRitual", "a/b/c", "/",
    };
    static const std::vector<const char*> sanev(sane, sane + 13);
    const double r = rng.uni01();
    if (r < 0.45) return rng.pick(sanev);
    if (r < 0.70) return randomGarbage(rng, 48);
    if (r < 0.85) {
        // Tag with many slashes (exercises the DirectiveResolved parsers).
        std::string s = randomGarbage(rng, 24);
        for (int i = 0; i < 1 + static_cast<int>(rng.below(6)); ++i)
            s.insert(rng.below(s.size() + 1), "/");
        return s;
    }
    // Huge tag (up to ~64KB).
    std::string s(1024 + rng.below(63 * 1024), 'A');
    for (size_t i = 0; i < s.size(); i += 997) s[i] = '/';
    return s;
}

std::string randomBytes(FuzzRng& rng, size_t len) {    std::string s;
    s.resize(len);
    for (size_t i = 0; i < len; ++i) s[i] = static_cast<char>(rng.below(256));
    return s;
}

std::string randomGarbage(FuzzRng& rng, size_t maxLen) {
    static const char pieces[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        " \t\r\n\v\f!@#$%^&*()-_=+[]{}|;:',.<>?/\\\"`~";
    // A few multi-byte / invalid UTF-8 sequences for good measure.
    static const char* const uni[] = {
        "\xC2\xA0",       // nbsp
        "\xE2\x80\x83",   // em space
        "\xF0\x9F\x98\x80", // emoji
        "\xED\xA0\x80",   // lone surrogate (invalid UTF-8)
        "\xFF\xFE",       // invalid bytes
    };
    std::string s;
    const size_t n = 1 + rng.below(maxLen);
    while (s.size() < n) {
        if (rng.chance(0.08))
            s += rng.pick(std::vector<const char*>(uni, uni + 5));
        else
            s += pieces[rng.below(sizeof(pieces) - 1)];
    }
    return s;
}

std::string mutateString(FuzzRng& rng, std::string s) {
    const int ops = 1 + static_cast<int>(rng.below(3));
    for (int k = 0; k < ops; ++k) {
        switch (rng.below(7)) {
            case 0: { // byte substitution (may inject NUL/high bytes)
                if (!s.empty())
                    s[rng.below(s.size())] = static_cast<char>(rng.below(256));
                break;
            }
            case 1: { // truncation (possibly to empty)
                if (!s.empty()) s.resize(rng.below(s.size() + 1));
                break;
            }
            case 2: { // span duplication
                if (!s.empty()) {
                    const size_t a = rng.below(s.size());
                    const size_t b = a + rng.below(s.size() - a + 1);
                    s.insert(rng.below(s.size() + 1), s.substr(a, b - a));
                }
                break;
            }
            case 3: { // token swap
                std::vector<std::string> toks;
                std::string cur;
                for (char c : s) {
                    if (c == ' ' || c == '\t') {
                        if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
                    } else {
                        cur += c;
                    }
                }
                if (!cur.empty()) toks.push_back(cur);
                if (toks.size() >= 2) {
                    const size_t a = rng.below(toks.size());
                    size_t b = rng.below(toks.size());
                    if (b == a) b = (b + 1) % toks.size();
                    std::swap(toks[a], toks[b]);
                    s.clear();
                    for (size_t i = 0; i < toks.size(); ++i) {
                        if (i) s += ' ';
                        s += toks[i];
                    }
                }
                break;
            }
            case 4: { // token insertion (numbers, slashes, quotes, verbs)
                static const char* const toks[] = {
                    "raid",   "war",    "convert", "sacrifice", "defend",
                    "relic",  "Fear",   "Dreams",  "-1",        "0",
                    "9999999999999999999", "1e999", "nan",       "inf",
                    "/",      "|",      ",",       "\"",        "'",
                    "...",    "--",     "..",      "~",
                };
                static const std::vector<const char*> tv(toks,
                                                         toks + 22);
                const char* t = rng.pick(tv);
                s.insert(rng.below(s.size() + 1), std::string(" ") + t);
                break;
            }
            case 5: { // whitespace mangling (incl. unicode spaces)
                static const char* const ws[] = {
                    " ", "\t", "\r", "\n", "\v", "\f", "  ",
                    "\xC2\xA0", "\xE2\x80\x83",
                };
                static const std::vector<const char*> wv(ws, ws + 9);
                s.insert(rng.below(s.size() + 1), rng.pick(wv));
                break;
            }
            default: { // case flip on a random alpha char
                for (size_t tries = 0; tries < 8 && !s.empty(); ++tries) {
                    const size_t i = rng.below(s.size());
                    char c = s[i];
                    if (c >= 'a' && c <= 'z') { s[i] = char(c - 32); break; }
                    if (c >= 'A' && c <= 'Z') { s[i] = char(c + 32); break; }
                }
                break;
            }
        }
    }
    return s;
}

std::string mutateLine(FuzzRng& rng,
                       const std::vector<std::string>& corpus) {
    const double r = rng.uni01();
    if (r < 0.60) {
        // Mutation of a valid command line.
        return mutateString(rng, rng.pick(corpus));
    }
    if (r < 0.72) {
        // Token soup: random words drawn from the corpus vocabulary.
        std::vector<std::string> words;
        for (const auto& line : corpus) {
            std::string cur;
            for (char c : line) {
                if (c == ' ') {
                    if (!cur.empty()) { words.push_back(cur); cur.clear(); }
                } else {
                    cur += c;
                }
            }
            if (!cur.empty()) words.push_back(cur);
        }
        std::string s;
        const size_t n = 1 + rng.below(6);
        for (size_t i = 0; i < n; ++i) {
            if (i) s += rng.chance(0.8) ? " " : "\t";
            s += rng.pick(words);
        }
        return s;
    }
    if (r < 0.82) return randomGarbage(rng, 256);        // pure garbage
    if (r < 0.90) return randomGarbage(rng, 10 * 1024);  // overlong (10KB)
    // Whitespace / unicode / empty edge cases.
    static const char* const edges[] = {
        "",      " ",       "\t",     "\n",    "\r\n",  "\v\f",
        "\xC2\xA0",        // nbsp only
        "\xE2\x80\x83",    // em space only
        "\xF0\x9F\x98\x80", // single emoji
        "\xED\xA0\x80",    // lone surrogate
        "quit\x00exit",    // embedded NUL (as C string this truncates;
                           // replaced below with a real embedded NUL)
    };
    const char* e = rng.pick(std::vector<const char*>(
        edges, edges + sizeof(edges) / sizeof(edges[0])));
    if (std::strcmp(e, "quit\x00exit") == 0)
        return std::string("quit\0exit", 9); // true embedded NUL
    return e;
}

std::string escapeForReport(const std::string& s, size_t maxLen) {
    std::string out;
    out.reserve(std::min(s.size(), maxLen) + 32);
    char buf[8];
    for (size_t i = 0; i < s.size() && out.size() < maxLen; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 32 && c < 127 && c != '\\') {
            out += static_cast<char>(c);
        } else {
            std::snprintf(buf, sizeof(buf), "\\x%02x", c);
            out += buf;
        }
    }
    if (s.size() > maxLen) {
        char tail[64];
        std::snprintf(tail, sizeof(tail), "...[len=%zu]", s.size());
        out += tail;
    }
    return out;
}

void reportFailure(const char* target, uint64_t seed, size_t iter,
                   const std::string& inputDesc, const std::string& what) {
    // Raw fd: bypasses the silencer's redirections so the report is
    // never swallowed.
    ::dprintf(g_reportFd,
              "\n[FUZZ-FAIL] target=%s seed=%llu iter=%zu\n"
              "  what: %s\n"
              "  input: %s\n",
              target, (unsigned long long)seed, iter, what.c_str(),
              inputDesc.c_str());
}

std::vector<std::string> driverCommandCorpus(bool includeBlockingNet) {
    std::vector<std::string> c = {
        "move n 3", "move sw 10", "move xyz", "camera fp", "camera tp",
        "camera xx", "look", "spawn cultist 2", "spawn sorcerer 1",
        "spawn monstrosity 3", "spawn civilian 4", "spawn dragon 5",
        "belief Fear", "belief nonsense", "belief Fear replace Dreams",
        "belief Torture replace", "beliefs", "rest 0", "rest 99",
        "command raid", "command war", "command convert",
        "command sacrifice", "command defend", "command relic",
        "command frobnicate", "attack", "cast fireball", "cast fear",
        "cast oops", "tick 10", "tick 3600", "tick -5", "tick 0",
        "save /tmp/cultulhu_fuzz.sav", "load /tmp/cultulhu_fuzz.sav",
        "save", "load", "myip", "host 47778 fuzz",
        "host 99999999999 x", "host -1", "ready", "players", "startgame",
        "startgame force", "chat hello world", "chat", "netent", "leave",
        "build wall", "build altar", "build oops", "menu", "menu cultist",
        "menu location", "menu enemy", "menu altar", "menu bogus", "combo",
        "chars", "addchar foo", "addchar /", "validate x", "kda",
        "interact", "jump", "sprint on", "sprint off", "sprint maybe",
        "rmb press", "rmb move 1 2", "rmb move nan inf", "rmb launch",
        "rmb release", "rmb stun", "rmb stun 42", "rmb stun 999999",
        "rmb bogus", "ambient", "ambient pray", "ambient frobnicate",
        "dungeon", "dungeon 42", "dungeon -7", "status", "help", "quit",
        "exit", "", "   ",
    };
    if (includeBlockingNet) {
        // Parse-only: never executed against live sockets by the dispatch
        // fuzz (they block by design), but the parsers must see them.
        c.push_back("discover 2");
        c.push_back("discover 999999");
        c.push_back("join 127.0.0.1 47778 bob");
        c.push_back("join 300.1.2.3 80 x");
        c.push_back("join");
    }
    return c;
}

} // namespace fuzz
