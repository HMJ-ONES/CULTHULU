#pragma once
// Wrappers around the real driver TU (src/driver/main.cpp) for the fuzz
// harness. The driver TU is compiled once (fuzz_driver_tu.cpp) with its
// main() renamed; these wrappers give the fuzz targets external-linkage
// access to the real dispatch, game object, and name parsers.

#include "commands/CommandSystem.h"
#include "core/EventBus.h"

#include <string>

// BetaGame / NetSession are defined at global scope in src/driver/main.cpp.
struct BetaGame;
struct NetSession;

namespace cultulhu {
enum class DirectiveType;
enum class Belief;
} // namespace cultulhu

namespace fuzz {

// Construct/destroy a fully-wired driver game (setupWorld() run).
BetaGame* newGame();
void deleteGame(BetaGame* g);
NetSession* newNetSession();
void deleteNetSession(NetSession* n);

// One REPL line through the REAL driver dispatch. Returns false on quit.
bool processLine(BetaGame& g, NetSession& n, const std::string& line);

// The driver's directive/belief name parsers (must never crash on garbage).
cultulhu::DirectiveType directiveByName(const std::string& name);
cultulhu::Belief beliefByName(const std::string& name);

// Subsystem access for the event-bus target (complete types stay in the
// driver TU; access goes through these).
cultulhu::EventBus& gameBus(BetaGame& g);
cultulhu::CommandSystem& gameCommands(BetaGame& g);

} // namespace fuzz
