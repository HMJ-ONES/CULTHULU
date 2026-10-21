#include "CultMatchUnit.h"
#include "CultMatchDirector.h"
#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCorpse.h"
#include "CultBlastFX.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

ACultMatchUnit::ACultMatchUnit() {
 PrimaryActorTick.bCanEverTick=true;
 Capsule=CreateDefaultSubobject<UCapsuleComponent>(TEXT("UnitCollision")); RootComponent=Capsule;
 Capsule->InitCapsuleSize(65,150); Capsule->SetCollisionObjectType(ECC_Pawn);
 Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Capsule->SetCollisionResponseToAllChannels(ECR_Ignore); Capsule->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}
void ACultMatchUnit::BeginPlay() {
 Super::BeginPlay(); MaxHealth=Health; Previous=GetActorLocation(); Phase=GetUniqueID()*.31f;
 const float Half=Kind==4?1600:Kind>=3?1250:Kind==2?240:150;
 Capsule->SetCapsuleSize(Kind>=3?450:Kind==2?150:65,Half);
 auto* Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface")),this);
 Mat->SetVectorParameterValue(TEXT("Color"),Team==0?FLinearColor(.045f,.32f,.5f):FLinearColor(.65f,.16f,.055f));
 auto Part=[&](const TCHAR* Mesh,FVector Pos,FVector Scale) {
  auto* C=NewObject<UStaticMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Mesh)); C->SetMaterial(0,Mat); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetRelativeLocation(Pos); C->SetRelativeScale3D(Scale); C->RegisterComponent(); Parts.Add(C); return C;
 };
 if(Kind>=3) {
  Part(TEXT("/Engine/BasicShapes/Cylinder"),FVector(0,0,-Half+120),FVector(11,11,2.4f));
  Part(TEXT("/Engine/BasicShapes/Cone"),FVector(0,0,-100),FVector(5,5,Half/65));
  auto* Eye=Part(TEXT("/Engine/BasicShapes/Sphere"),FVector(0,0,Half*.6f),FVector(Kind==4?8:3));
  Eye->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_Glow_Eye")));
  for(int I=0;I<4;++I) { float A=I*PI*.5f; Part(TEXT("/Engine/BasicShapes/Cube"),FVector(FMath::Cos(A)*460,FMath::Sin(A)*460,-Half*.3f),FVector(1.5f,1.5f,Half/75)); }
 } else {
  Part(TEXT("/Engine/BasicShapes/Cone"),FVector(0,0,-15),FVector(1.2f,1.0f,1.7f));
  Part(TEXT("/Engine/BasicShapes/Sphere"),FVector(0,0,90),FVector(.65f));
  Part(TEXT("/Engine/BasicShapes/Cube"),FVector(15,-65,10),FVector(.3f,.3f,1.0f));
  Part(TEXT("/Engine/BasicShapes/Cube"),FVector(15,65,10),FVector(.3f,.3f,1.0f));
  Part(TEXT("/Engine/BasicShapes/Cube"),FVector(0,-28,-100),FVector(.3f,.35f,.8f));
  Part(TEXT("/Engine/BasicShapes/Cube"),FVector(0,28,-100),FVector(.3f,.35f,.8f));
  if(Kind==1) Part(TEXT("/Engine/BasicShapes/Cylinder"),FVector(35,80,60),FVector(.16f,.16f,2.8f));
  if(Kind==2) { Part(TEXT("/Engine/BasicShapes/Sphere"),FVector(-100,0,-45),FVector(4,2,2)); Part(TEXT("/Engine/BasicShapes/Cylinder"),FVector(90,0,40),FVector(.8f,.8f,3)); }
 }
 if(Kind<2) CreateArtBody();
}
FString ACultMatchUnit::Title() const { return Kind==5?TEXT("NEXUS WARD"):Kind==4?TEXT("GREAT OLD ONE"):Kind==3?TEXT("WARD TOWER"):Kind==2?TEXT("SIEGE ACOLYTE"):Kind==1?TEXT("HEX ACOLYTE"):TEXT("CULT VANGUARD"); }
void ACultMatchUnit::Tick(float Dt) {
 Super::Tick(Dt); if(Health<=0) return; if(UpdateTransit(Dt)) return; Control.Tick(Dt); AttackCooldown=FMath::Max(0.f,AttackCooldown-Dt);
 if(Life>0) { Life-=Dt; if(Life<=0) { Health=0; Die(); return; } }
 const float Walk=FVector::Dist2D(Previous,GetActorLocation()); Previous=GetActorLocation(); Phase+=Dt*8; AnimateArtBody(Dt,Walk>1);
 if(!IsStructure()&&Parts.Num()>=6) for(int I=2;I<6;++I) Parts[I]->SetRelativeRotation(FRotator(Walk>1?FMath::Sin(Phase+(I%2)*PI)*30:0,0,0));
}
void ACultMatchUnit::Hurt(float Amount,AActor* Source) {
 if(Health<=0||Amount<=0||IsInTransit()) return;
 if(auto* D=ACultMatchDirector::Find(GetWorld())) Health=D->HurtUnit(this,Amount,Source);
 if(Health<=0) Die();
}
void ACultMatchUnit::Die() {
 for(auto* C:Parts) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 ACultCorpse::Leave(this); ACultBlastFX::SpawnBlast(GetWorld(),ECultBlastKind::ShockRing,GetActorLocation(),IsStructure()?8:1); Destroy();
}
void ACultMatchUnit::MoveTo(FVector Point,float Dt) {
 if(IsStructure()||Control.Stop||IsInTransit()) return;
 FVector To=Point-GetActorLocation(); To.Z=0;
 if(Control.Fear) To=GetActorLocation()-Control.Origin;
 if(To.Size2D()<100) return;
 const FVector Direction=To.GetSafeNormal2D();
 const float Step=FMath::Min(float(To.Size2D()),Speed*Control.Speed*Dt);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(MatchMinionMove),false,this);
 const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
 const FCollisionObjectQueryParams Static(ECC_WorldStatic);
 for(float Turn:{0.f,25.f,-25.f,55.f,-55.f,90.f,-90.f}) {
  const FVector Heading=Direction.RotateAngleAxis(Turn,FVector::UpVector);
  const FVector Raised=GetActorLocation()+FVector(0,0,200),Next=Raised+Heading*Step; FHitResult Hit;
  if(GetWorld()->SweepSingleByObjectType(Hit,GetActorLocation(),Raised,FQuat::Identity,Static,Shape,Query)) continue;
  if(GetWorld()->SweepSingleByObjectType(Hit,Raised,Next,FQuat::Identity,Static,Shape,Query)) continue;
  if(!GetWorld()->SweepSingleByObjectType(Hit,Next,Next-FVector(0,0,500),FQuat::Identity,Static,Shape,Query)||Hit.bStartPenetrating||Hit.ImpactNormal.Z<.65f) continue;
  SetActorLocation(Hit.Location+FVector(0,0,3),false); SetActorRotation(Heading.Rotation()); break;
 }
}
void ACultMatchUnit::FireAt(AActor* Target) {
 if(Health<=0||IsInTransit()||AttackCooldown>0||Control.Stop||Control.Fear||!ACultSkillAttack::Alive(Target)) return;
 FVector Origin=GetActorLocation(); if(IsStructure()) Origin.Z+=500;
 FVector Aim=Target->GetActorLocation();
 if(auto* Cap=Target->FindComponentByClass<UCapsuleComponent>()) { FVector Closest; if(Cap->GetClosestPointOnCollision(Origin,Closest)>=0) Aim=Closest; }

 const FTransform T(Origin); auto* A=GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!A) return;
 const bool Ranged=Kind>0;
 A->Shape=Ranged?ACultSkillAttack::EShape::Bolt:ACultSkillAttack::EShape::Sweep;
 A->Direction=(Aim-Origin).GetSafeNormal(); A->Reach=Range+1500; A->Radius=Ranged?45:55; A->Speed=IsStructure()?15000:2200; A->Windup=IsStructure()?.25f:.28f; A->Duration=Ranged?A->Reach/A->Speed+.1f:.18f;
 if(!Ranged) { A->Reach=Range; A->HalfAngle=50; }
 TWeakObjectPtr<ACultMatchUnit> Weak(this); const float Damage=Dps;
 A->Hit=[Weak,Damage](AActor* Other,FVector Point) { if(Weak.IsValid()) ACultSkillAttack::Hurt(Other,Damage,Weak.Get(),Point); };
 A->FinishSpawning(T); AttackCooldown=1.f;
}
