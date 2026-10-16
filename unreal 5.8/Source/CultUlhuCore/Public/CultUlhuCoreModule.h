// CULT-ULHU core wrapper module interface.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class CULTULHUCORE_API FCultUlhuCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
