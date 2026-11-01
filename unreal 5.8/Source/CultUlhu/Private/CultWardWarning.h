#pragma once
#include "CoreMinimal.h"
class ACultUlhuPlayerPawn;
class ACultSkillAttack;
class UCanvas;
struct FCultWardWarning {
 TWeakObjectPtr<ACultSkillAttack> Shot;
 float ReleaseIn=0;
 FVector Source=FVector::ZeroVector;
 bool Incoming=false;
 bool IsVisible() const { return Shot.IsValid(); }
};
struct FCultWardWarningPanel {
 FBox2D Bounds=FBox2D(ForceInit);
 FString Title,Detail;
 float TitleHeight=0;
};
namespace CultWardWarning {
 // Only a real, living ward's lock on this player warrants a screen warning.
 FCultWardWarning Observe(ACultUlhuPlayerPawn* Player);
 FBox2D Bounds(FVector2D Canvas,float Scale,FVector2D TextSize);
 FCultWardWarningPanel Panel(const FCultWardWarning& Warning,float DashCooldown,UCanvas* Canvas);
}
