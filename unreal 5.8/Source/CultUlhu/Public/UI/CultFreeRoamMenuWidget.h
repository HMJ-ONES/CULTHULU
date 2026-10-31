#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CultFreeRoamMenuWidget.generated.h"

UCLASS()
class CULTULHU_API UCultFreeRoamMenuWidget : public UUserWidget {
 GENERATED_BODY()
public:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
 void Status();
 UFUNCTION() void Resume();
 UFUNCTION() void Save();
 UFUNCTION() void Load();
 UFUNCTION() void NewGame();
 UFUNCTION() void Host();
 UFUNCTION() void Join();
 UFUNCTION() void Exit();
 UPROPERTY() class UTextBlock* Status_=nullptr;
 UPROPERTY() class UEditableTextBox* Address_=nullptr;
};
