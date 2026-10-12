#pragma once

#include <string>
#include <vector>

namespace cultulhu {

// Suggests the most likely intended driver command for a mistyped input.
// Compares `input` against `commands` by edit distance and returns the
// closest candidate, or an empty string when nothing is within 2 edits
// (or `input`/`commands` is empty). Ties resolve to the first candidate
// in list order, so results are deterministic.
std::string suggestCommand(const std::string& input,
                           const std::vector<std::string>& commands);

} // namespace cultulhu
