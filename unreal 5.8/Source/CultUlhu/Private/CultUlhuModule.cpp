// CULT-ULHU primary game module entry point.
// Every UE module needs exactly one IMPLEMENT_* macro or the engine
// refuses to load it at startup ("The game module 'CultUlhu' could not
// be loaded"). FDefaultGameModuleImpl is enough: all CULT-ULHU behavior
// lives in the GameMode/PlayerController classes, no custom startup needed.
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CultUlhu, "CultUlhu");
