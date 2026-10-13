#pragma once
// CULT-ULHU micro-dev (2026-10-13): the 30s character-select window used to
// close with no warning — a player who started a match and drifted off got
// dumped back to free-roam without ever learning a countdown existed. The
// driver now names the window in the match-start line and prints one nudge
// 10s before it closes. This header holds the nudge's boundary logic as a
// pure predicate so the timing contract is pinned by regression tests; the
// driver passes steady_clock times plus its own announced flag.
#include <chrono>

namespace cultulhu {
namespace driver {

// Wave 42: character select countdown — 30s from match start to pick a
// fighter. Miss it and you're back in free-roam.
inline constexpr std::chrono::seconds kSelectWindow{30};
// Micro-dev (2026-10-13): the nudge fires this far before the window closes.
inline constexpr std::chrono::seconds kSelectNudgeLead{10};

// True once while the window is open and within kSelectNudgeLead of
// closing — the driver sets alreadyNudged after printing, so the nudge
// fires exactly once per match. The >= kSelectWindow boundary belongs to
// the window-closed message, never the nudge.
inline bool selectNudgeDue(std::chrono::steady_clock::time_point start,
                           std::chrono::steady_clock::time_point now,
                           bool alreadyNudged) {
    if (alreadyNudged) return false;
    const auto elapsed = now - start;
    return elapsed >= kSelectWindow - kSelectNudgeLead &&
           elapsed < kSelectWindow;
}

} // namespace driver
} // namespace cultulhu
