#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CultHostedRoomWidget.generated.h"
UCLASS()
class CULTULHU_API UCultHostedRoomWidget:public UUserWidget {
 GENERATED_BODY()
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeTick(const FGeometry&,float) override;
 UPROPERTY() class UTextBlock* Status_=nullptr;
 UFUNCTION() void Rejoin(); UFUNCTION() void Copy(); UFUNCTION() void Close();
};
