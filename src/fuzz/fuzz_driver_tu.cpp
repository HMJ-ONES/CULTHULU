// Fuzzing seam for the driver (src/driver/main.cpp, wave 9d).
//
// The whole driver translation unit is compiled here with its main()
// renamed, so this binary keeps its own main(). The fuzz targets get the
// REAL processLine / BetaGame / NetSession / name parsers with zero logic
// duplication: any dispatch bug the fuzzer finds is a bug in the actual
// driver, not in a reimplementation.
//
// NOTE: this TU must be compiled exactly once in the fuzz binary (it
// defines the driver's globals/statics).

#define main cultulhu_driver_main
#include "driver/main.cpp"
#undef main

#include "fuzz/fuzz_driver.h"

namespace fuzz {

BetaGame* newGame() {
    BetaGame* g = new BetaGame();
    g->setupWorld();
    return g;
}

void deleteGame(BetaGame* g) { delete g; }

NetSession* newNetSession() { return new NetSession(); }

void deleteNetSession(NetSession* n) { delete n; }

bool processLine(BetaGame& g, NetSession& n, const std::string& line) {
    return ::processLine(g, n, line);
}

cultulhu::DirectiveType directiveByName(const std::string& name) {
    return ::directiveByName(name);
}

cultulhu::Belief beliefByName(const std::string& name) {
    return ::beliefByName(name);
}

cultulhu::EventBus& gameBus(BetaGame& g) { return g.bus; }

cultulhu::CommandSystem& gameCommands(BetaGame& g) { return g.commands; }

} // namespace fuzz
