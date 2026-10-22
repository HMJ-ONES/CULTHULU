#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CultPlayHUD.generated.h"

class ACultPopulation;
class ACultUlhuPlayerPawn;
struct FCultInspectionCard {
 TWeakObjectPtr<ACultPopulation> Person;
 FBox2D Bounds;
 FVector2D Anchor;
 FString Activity,Identity,Contribution;
 float Health=0,MaxHealth=1;
 bool Selected=false,Hovered=false;
 FLinearColor Accent;
};

UCLASS()
class CULTULHU_API ACultPlayHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	void CommandClick();
 void ToggleFaith();
 bool FaithVisible() const { return bFaith_; }
 void ScrollFaith(int Lines) { FaithScroll_=FMath::Max(0,FaithScroll_+Lines); }
	void CloseCommands() { bCommands_=false; bSelecting_=false; }
 void CommandRelease();
 void DrawSelection();
 const TArray<FCultInspectionCard>& InspectionCards() const { return InspectionCards_; }
 float CombatContentTop() const { return CombatBottomTop_; }
private:
 friend void DrawCultCombat(ACultPlayHUD*,ACultUlhuPlayerPawn*,UCanvas*);
 friend struct FCultHUDCoordinateTestAccess;
 float CombatBottomTop_=MAX_flt;
 TArray<FCultInspectionCard> InspectionCards_;
 void DrawPopulationInspection();
	bool bCommands_=false;
 bool bSelecting_=false;
 FVector2D SelectionStart_;
 void OpenCommands(float X,float Y,AActor* SelectedEnemy=nullptr);
	FVector CommandPoint_;
 FVector2D CommandScreen_;
	TWeakObjectPtr<AActor> CommandTarget_;
	TArray<int32> Actions_;
	TArray<FBox2D> Buttons_;
	TArray<FString> Labels_;
	TArray<bool> Enabled_;
 bool bFaith_=false;
 bool bFaithOutcomes_=false;
 int32 SelectedBelief_=11,FaithScroll_=0;
 TArray<FBox2D> FaithButtons_;
 TArray<FString> FaithActions_;
 void DrawFaith();
 void DrawSociety();
 void FaithClick(float X,float Y);
};
