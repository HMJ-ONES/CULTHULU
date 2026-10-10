#pragma once
// The three wave-9d fuzz targets. Each runs `iterations` iterations with a
// deterministic input stream derived from `seed`, prints the seed up front,
// and returns 0 (clean) or 1 (bug found: crash/exception/hang reproduced).
// On a bug it prints the exact failing input + seed before returning.

#include <cstddef>
#include <cstdint>

namespace fuzz {

int runTarget1(uint64_t seed, size_t iterations); // event bus
int runTarget2(uint64_t seed, size_t iterations); // command/directive parser
int runTarget3(uint64_t seed, size_t iterations); // driver REPL dispatch

} // namespace fuzz
