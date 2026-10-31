#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultSkillAttack.generated.h"

// Transient, visible geometry shared by player, bot and creature attacks.
UCLASS()
class CULTULHU_API ACultSkillAttack : public AActor {
 GENERATED_BODY()
public:
 ACultSkillAttack();
 enum class EShape { Bolt, Sweep, Sphere, GroundDisk, Cone, Beam };
 EShape Shape=EShape::Bolt;
 float Windup=.18f, Duration=.22f, Radius=35, Reach=2000, Speed=2400, HalfAngle=45;
 FVector Direction=FVector::ForwardVector;
 // Only ward shots use a target lock; character and minion bolts remain physical skill shots.
 TWeakObjectPtr<AActor> WardTarget;
 bool WardLocked=false;
 TFunction<bool(AActor*)> TargetFilter;
 FVector ProjectilePosition() const { return Previous; }
 TFunction<void(AActor*,FVector)> Hit;
 TFunction<void(FVector)> Finished;
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 bool IsWindingUp() const { return Age<Windup; }
 float WindupProgress() const { return Windup>0?FMath::Clamp(Age/Windup,0.f,1.f):1.f; }
 float TimeUntilActive() const { return FMath::Max(0.f,Windup-Age); }
 static float HealthOf(AActor* Actor);
 static bool Alive(AActor* Actor);
 static bool Enemy(AActor* Source,AActor* Other);
 static bool Clear(UWorld* W,FVector From,FVector To,AActor* Source,AActor* Target);
 static bool AreaContact(EShape Kind,FVector Origin,FVector Direction,float Reach,float Radius,float HalfAngle,AActor* Target,FVector& Point);
 static void Hurt(AActor* Target,float Damage,AActor* Source,FVector Point);
private:
 friend struct FCultAttackPresentationTestAccess;
 float Age=0,Travel=0;
 float BornAt=0;
 bool FirstTick=true;
 FVector Origin,Previous,SweepOffset;
 TSet<TWeakObjectPtr<AActor>> Struck;
 UPROPERTY() class UStaticMeshComponent* Visual=nullptr;
 UPROPERTY() TArray<class UStaticMeshComponent*> Ring;
 UPROPERTY() class UProceduralMeshComponent* Telegraph=nullptr;
 UPROPERTY() class UMaterialInstanceDynamic* SignalMaterial=nullptr;
 void BuildPresentation();
 void UpdatePresentation();
 void BuildWarningGeometry(bool Trail=false);
 void Sweep(FVector A,FVector B,float R);
 void Area();
 void ShowSegment(FVector A,FVector B,float Width);
};
