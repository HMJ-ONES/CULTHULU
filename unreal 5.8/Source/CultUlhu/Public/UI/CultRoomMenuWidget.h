#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CultRoomMenuWidget.generated.h"
UCLASS()
class CULTULHU_API UCultRoomMenuWidget:public UUserWidget {
 GENERATED_BODY()
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeTick(const FGeometry&,float) override;
 UPROPERTY() class UTextBlock* Status_=nullptr;
 UPROPERTY() class UEditableTextBox* Code_=nullptr;
 UFUNCTION() void Find(); UFUNCTION() void Host(); UFUNCTION() void Private(); UFUNCTION() void Join(); UFUNCTION() void Offline(); UFUNCTION() void Continue(); UFUNCTION() void Back();
};
