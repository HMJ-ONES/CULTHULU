#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CultBeliefSelectWidget.generated.h"
class UCultBeliefSelectWidget;
UCLASS()
class CULTULHU_API UCultBeliefChoiceButton : public UButton
{
 GENERATED_BODY()
public:
 void Setup(int32 Id,UCultBeliefSelectWidget* Owner);
private:
 int32 Id_=0;
 UPROPERTY() UCultBeliefSelectWidget* Owner_=nullptr;
 UFUNCTION() void Choose();
};
UCLASS()
class CULTULHU_API UCultBeliefSelectWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 void ToggleChoice(int32 Id);
 const TArray<int32>& Choices() const { return Choices_; }
 bool CanEnter() const { return Choices_.Num()==3; }
 UFUNCTION() void Enter();
private:
 TArray<int32> Choices_;
 UPROPERTY() TArray<UCultBeliefChoiceButton*> Buttons_;
 UPROPERTY() class UTextBlock* Count_=nullptr;
 UPROPERTY() class UTextBlock* Detail_=nullptr;
 UPROPERTY() UButton* Enter_=nullptr;
 UFUNCTION() void Back();
 void Refresh();
};
