#pragma once
// Shared utilities for the CULT-ULHU fuzz harness (src/fuzz, wave 9d).
//
// Deterministic input generation (std::mt19937_64, NOT the game's RNG, so a
// printed seed fully determines the input stream), string mutation, a
// per-iteration chrono watchdog, cout/cerr silencing, and failure reports.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <streambuf>
#include <string>
#include <unistd.h>
#include <vector>

namespace fuzz {

// ------------------------------------------------------------------- PRNG
// Deterministic PRNG for *input generation*. Kept separate from the game's
// RNG so the harness seed alone reproduces any run.
class FuzzRng {
public:
    explicit FuzzRng(uint64_t seed) : eng_(seed) {}
    uint64_t u64() { return eng_(); }
    uint32_t u32() { return static_cast<uint32_t>(eng_()); }
    double uni01() {
        return std::uniform_real_distribution<double>(0.0, 1.0)(eng_);
    }
    int64_t range(int64_t lo, int64_t hi) {
        return std::uniform_int_distribution<int64_t>(lo, hi)(eng_);
    }
    size_t below(size_t n) {
        return n == 0 ? 0 : static_cast<size_t>(eng_() % n);
    }
    bool chance(double p) { return uni01() < p; }
    template <typename T>
    const T& pick(const std::vector<T>& v) {
        return v[below(v.size())];
    }

private:
    std::mt19937_64 eng_;
};

// ------------------------------------------------- value generation -----
// Extreme-value float generator: sane values, negatives, +-Inf, NaN,
// huge (1e30), denormals.
float randomFloat(FuzzRng& rng);
// Event tag generator: empty, sane tags, garbage, slash-heavy, huge.
std::string randomTag(FuzzRng& rng);

// ------------------------------------------------- string generation -----
// Random bytes (may include NULs, invalid UTF-8, control chars).
std::string randomBytes(FuzzRng& rng, size_t len);
// Random "text-ish" garbage: printable runs mixed with whitespace,
// high bytes, and invalid UTF-8 sequences.
std::string randomGarbage(FuzzRng& rng, size_t maxLen);
// Mutate a valid command line: byte substitution, truncation, span
// duplication, token swaps, token insertion, whitespace mangling.
std::string mutateString(FuzzRng& rng, std::string s);
// Full line generator: mutated valid commands, token soup, pure garbage,
// overlong inputs (up to 10KB), and whitespace/unicode edge cases.
std::string mutateLine(FuzzRng& rng, const std::vector<std::string>& corpus);

// -------------------------------------------------------------- watchdog
// Per-iteration hang detector. The harness checks elapsedMs() after every
// iteration; anything over kHangThresholdMs aborts the target and is
// reported as a hang bug. Uses steady_clock inside the loop (no signals).
struct Watchdog {
    using clock = std::chrono::steady_clock;
    static constexpr double kHangThresholdMs = 500.0;
    clock::time_point t0_;
    void start() { t0_ = clock::now(); }
    double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(clock::now() - t0_)
            .count();
    }
};

// ------------------------------------------------------- output control
// Redirects std::cout/std::cerr to a null sink AND the raw stderr file
// descriptor (fd 2) to /dev/null for the duration of a fuzz run: the
// driver is chatty on both iostreams and raw fprintf(stderr, ...).
// Failure reports go to the saved stderr fd, so they are never swallowed.
class StreamSilencer {
public:
    StreamSilencer()
        : coutBuf_(std::cout.rdbuf()), cerrBuf_(std::cerr.rdbuf()),
          errFd_(::dup(2)) {
        std::cout.rdbuf(&null_);
        std::cerr.rdbuf(&null_);
        ::fflush(stderr);
        const int devNull = ::open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            ::dup2(devNull, 2);
            ::close(devNull);
        }
    }
    ~StreamSilencer() {
        ::fflush(stderr);
        if (errFd_ >= 0) {
            ::dup2(errFd_, 2);
            ::close(errFd_);
        }
        std::cout.rdbuf(coutBuf_);
        std::cerr.rdbuf(cerrBuf_);
    }
    StreamSilencer(const StreamSilencer&) = delete;
    StreamSilencer& operator=(const StreamSilencer&) = delete;

    // Raw fd for failure reports (bypasses both redirections).
    int reportFd() const { return errFd_; }

private:
    struct NullBuf : std::streambuf {
        int overflow(int c) override { return c; }
    } null_;
    std::streambuf* coutBuf_;
    std::streambuf* cerrBuf_;
    int errFd_;
};

// ---------------------------------------------------- failure reporting
// Escape control bytes for a one-line report; truncate very long inputs.
std::string escapeForReport(const std::string& s, size_t maxLen = 300);
// Direct failure reports at a raw fd (the silencer's saved stderr).
void setReportFd(int fd);
// Harness progress logging: same fd, printf-style. Never throws.
void logf(const char* fmt, ...);
// Prints seed + iteration + failing input. Never throws.
void reportFailure(const char* target, uint64_t seed, size_t iter,
                   const std::string& inputDesc, const std::string& what);

// ---------------------------------------------------------- corpora
// Valid driver command lines for mutation. includeBlockingNet adds the
// `discover`/`join` verbs (parse-only use); the live dispatch fuzz excludes
// them because they block on network I/O by design (beacon listen up to
// 15s, TCP connect timeout 5s) and would trip the hang watchdog without
// exercising any game parsing.
std::vector<std::string> driverCommandCorpus(bool includeBlockingNet);

} // namespace fuzz
