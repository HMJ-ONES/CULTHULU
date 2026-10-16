// CULT-ULHU core wrapper module implementation.
#include "CultUlhuCoreModule.h"

void FCultUlhuCoreModule::StartupModule()
{
	// Nothing to do: the core sim is instantiated per-subsystem.
	// VERIFY IN EDITOR: module loads at Startup (check Output Log for errors).
}

void FCultUlhuCoreModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FCultUlhuCoreModule, CultUlhuCore)
