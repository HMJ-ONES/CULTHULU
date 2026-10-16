// Wave 9c driver commands: `ambient` and `dungeon`.
//
// Stub translation unit. src/driver/ is excluded from the engine library's
// CMake source glob, so this module is header-inline: the real
// implementation lives in driver/wave9_content_cmds.h and compiles into
// cultulhu_play through main.cpp's #include of that header. This file
// exists so the .h/.cpp pair is complete and to give the module a home
// if driver-local (non-inline) helpers are ever needed.

#include "driver/wave9_content_cmds.h"
