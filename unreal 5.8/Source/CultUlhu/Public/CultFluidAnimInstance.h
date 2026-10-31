#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "CultFluidAnimInstance.generated.h"

// Native single-clip playback with pose-continuous transitions; no extra animation Blueprint asset.
UCLASS(Transient)
class CULTULHU_API UCultFluidAnimInstance : public UAnimSingleNodeInstance {
 GENERATED_BODY()
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
