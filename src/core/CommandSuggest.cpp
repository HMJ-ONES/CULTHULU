// Typo-tolerant "did you mean" suggestions for the driver REPL.
// Keeps the happy path cheap: edit distance is capped at the suggestion
// threshold (2), so far-away candidates bail out row by row instead of
// running the full matrix.

#include "core/CommandSuggest.h"

namespace cultulhu {

namespace {

// Edit distance, but returns maxDist + 1 as soon as it is certain the true
// distance exceeds maxDist. Input is const-referenced; no copies.
int cappedEditDistance(const std::string& a, const std::string& b, int maxDist) {
    const size_t n = a.size(), m = b.size();
    if (n > m) return cappedEditDistance(b, a, maxDist); // iterate the short side
    if (m - n > static_cast<size_t>(maxDist)) return maxDist + 1; // length reject
    std::vector<int> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= n; ++i) {
        cur[0] = static_cast<int>(i);
        int rowMin = cur[0];
        for (size_t j = 1; j <= m; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int v = prev[j - 1] + cost;
            if (prev[j] + 1 < v) v = prev[j] + 1;
            if (cur[j - 1] + 1 < v) v = cur[j - 1] + 1;
            cur[j] = v;
            if (v < rowMin) rowMin = v;
        }
        if (rowMin > maxDist) return maxDist + 1; // hopeless: stop early
        prev.swap(cur);
    }
    return prev[m];
}

} // namespace

std::string suggestCommand(const std::string& input,
                           const std::vector<std::string>& commands) {
    static const int kMaxDistance = 2; // beyond this, don't guess
    if (input.empty() || commands.empty()) return {};
    std::string best;
    int bestDist = kMaxDistance + 1;
    for (const std::string& cmd : commands) {
        if (cmd == input) return cmd;
        const int d = cappedEditDistance(input, cmd, kMaxDistance);
        if (d < bestDist) { // strict: first-in-list wins ties
            bestDist = d;
            best = cmd;
        }
    }
    return bestDist <= kMaxDistance ? best : std::string();
}

} // namespace cultulhu
