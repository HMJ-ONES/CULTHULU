#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "CultFluidAnimInstance.generated.h"

// Native single-clip playback with pose-continuous transitions; no extra animation Blueprint asset.
UCLASS(Transient)
class CULTULHU_API UCultFluidAnimInstance : public UAnimSingleNodeInstance {
 GENERATED_BODY()
public:
 float Gather=0, Snap=0, Guard=0;
 uint8 GestureRig=0;
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
