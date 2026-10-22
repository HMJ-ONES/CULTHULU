#pragma once
#include "CoreMinimal.h"
#include "characters/CharacterDef.h"

// Read the same definitions used by the original headless game.
namespace CultCanonicalKit
{
 CULTULHU_API bool IsReleased(const FString& Id);
 CULTULHU_API bool Load(const FString& Id, cultulhu::CharacterDef& Out);
 CULTULHU_API FString Summary(const FString& Id);
}
