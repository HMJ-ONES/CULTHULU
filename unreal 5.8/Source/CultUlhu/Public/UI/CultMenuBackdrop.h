#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "CultMenuBackdrop.generated.h"
UCLASS()
class CULTULHU_API UCultMenuBackdrop:public UWidget {
 GENERATED_BODY()
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
};
