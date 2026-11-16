#include "CultMatchState.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "discovery/RelicNames.h"

namespace {
bool WithinReach(FVector Feet,FVector Relic) {
 return FMath::Abs(Feet.Z-Relic.Z)<=400.f&&FVector::DistSquared2D(Feet,Relic)<=FMath::Square(1200.f);
}
}

int32 ACultMatchDirector::NearbyRelic(const ACultUlhuPlayerPawn* P) const {
 if(!P||!State_||!State_->Moba) return INDEX_NONE;
 int32 Best=INDEX_NONE;
 double Distance=TNumericLimits<double>::Max();
 const FVector Feet=P->FeetLocation();
 for(int32 I=0;I<State_->Relics.Num();++I) {
  const auto& R=State_->Relics[I];
  if(R.Claim>=0||State_->Time<R.Spawn||(R.IsRoving()&&R.Point.IsZero())||!WithinReach(Feet,R.Point)) continue;
  const double Current=FVector::DistSquared2D(Feet,R.Point);
  if(Current<Distance) { Best=I; Distance=Current; }
 }
 return Best;
}

void ACultMatchDirector::InterruptRelicClaim(ACultUlhuPlayerPawn* P,bool Hit) {
 if(!HasAuthority()||!State_||!State_->Moba) return;
 const int32 Index=Slot(P);
 if(Index<0) return;
 auto& Fighter=State_->Fighters[Index];
 if(!Hit) Fighter.RelicNeedsRelease=false;
 for(auto& R:State_->Relics) if(R.ChannelSlot==Index&&R.Claim<0) {
  R.Channel=0;
  R.ChannelSlot=-1;
  if(Hit) {
   Fighter.RelicBlockedUntil=State_->Time+3.f;
   Fighter.RelicNeedsRelease=!P->IsBot();
  }
 }
}

void ACultMatchDirector::UpdateRelics(float Dt) {
 auto& S=*State_;
 for(int32 Index=0;Index<S.Relics.Num();++Index) {
  auto& R=S.Relics[Index];
  if(R.Claim>=0||S.Time<R.Spawn) continue;
  if(R.Name.EndsWith(TEXT("relic"))) R.Name=UTF8_TO_TCHAR(cultulhu::generateRelicName(S.Rng).c_str());
  if(R.IsRoving()&&R.Point.IsZero()) {
   const bool North=FMath::RandBool();
   R.Point=FVector(0,North?-8500.f:8500.f,0);
   S.RelicNotice=North?TEXT("VEILHEART HAS MANIFESTED | NORTH JUNGLE"):TEXT("VEILHEART HAS MANIFESTED | SOUTH JUNGLE");
   S.RelicNoticeUntil=S.Time+10.f;
   UE_LOG(LogTemp,Display,TEXT("[CultMatch] %s"),*S.RelicNotice);
  } else if(R.Tier==3&&!R.Visual.IsValid()) {
   Announce(TEXT("THE MYTHIC RELIC HAS MANIFESTED AT MID | LEVEL 30, HOLD E FOR 8S"));
  }
  int32 Candidate=INDEX_NONE,Teams=0;
  for(int32 I=0;I<S.Fighters.Num();++I) {
   const auto& F=S.Fighters[I];
   const auto* Pawn=F.Pawn.Get();
   if(!Pawn||F.Respawn>0||Pawn->GetHealth()<=0||!WithinReach(Pawn->FeetLocation(),R.Point)) continue;
   Teams|=1<<F.Team;
   const bool Wants=Pawn->IsBot()?(I%5<2&&F.RouteStep>=2&&!F.Retreating):Pawn->IsHoldingInteract();
   if(Wants&&!F.RelicNeedsRelease&&S.Time>=F.RelicBlockedUntil&&F.Level>=R.RequiredLevel()&&
      !Pawn->IsIncapacitated()&&!Pawn->IsDashing()&&NearbyRelic(Pawn)==Index&&
      (Candidate==INDEX_NONE||R.ChannelSlot==I)) Candidate=I;
  }
  R.Contested=Teams==3;
  if(R.Contested||Candidate==INDEX_NONE) {
   R.Channel=0;
   R.ChannelSlot=INDEX_NONE;
   continue;
  }
  if(R.ChannelSlot!=Candidate) { R.ChannelSlot=Candidate; R.Channel=0; }
  R.Channel+=Dt;
  if(R.Channel<R.Need) continue;
  auto& F=S.Fighters[Candidate];
  F.RelicMult=FMath::Min(3.f,F.RelicMult+R.Amp);
  Announce(F.Name+TEXT(" claimed ")+R.Name);
  R.Claim=Candidate;
  R.Channel=0;
  R.ChannelSlot=INDEX_NONE;
  if(R.IsRoving()) {
   R.Point=FVector::ZeroVector;
   R.Claim=INDEX_NONE;
   R.Spawn=S.Time+FMath::FRandRange(180.f,240.f);
   S.RelicNoticeUntil=0;
  }
 }
 RefreshRelicVisuals();
}

void ACultMatchDirector::RefreshRelicVisuals() {
 if(!State_) return;
 for(auto& R:State_->Relics) {
  const bool Visible=R.Claim<0&&State_->Time>=R.Spawn&&(!R.IsRoving()||!R.Point.IsZero());
  if(!Visible) {
   if(R.Visual.IsValid()) R.Visual->Destroy();
   R.Visual.Reset();
   continue;
  }
  if(!R.Visual.IsValid()) {
   auto* Actor=GetWorld()->SpawnActor<AStaticMeshActor>(R.Point+FVector(0,0,340),FRotator::ZeroRotator);
   if(!Actor) continue;
   auto* Mesh=Actor->GetStaticMeshComponent();
   Mesh->SetMobility(EComponentMobility::Movable);
   Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere")));
   Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Mesh->SetCastShadow(false);
   if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AttackSignal"))) Mesh->SetMaterial(0,UMaterialInstanceDynamic::Create(Base,Actor));
   R.Visual=Actor;
  }
  const float Progress=FMath::Clamp(R.Channel/FMath::Max(1.f,R.Need),0.f,1.f);
  auto* Actor=CastChecked<AStaticMeshActor>(R.Visual.Get());
  const float Scale=(R.IsRoving()?5.f:R.Tier+2.f)*(1.f+.7f*Progress);
  Actor->SetActorScale3D(FVector(Scale,Scale,Scale*1.2f));
  if(auto* Glow=Cast<UMaterialInstanceDynamic>(Actor->GetStaticMeshComponent()->GetMaterial(0))) {
   const FLinearColor Idle=R.IsRoving()?FLinearColor(.55f,.16f,1.f):FLinearColor(1.f,.51f,.10f);
   Glow->SetVectorParameterValue(TEXT("Color"),R.Contested?FLinearColor(1.f,.12f,.07f):FMath::Lerp(Idle,FLinearColor(1.f,.95f,.74f),Progress));
   Glow->SetScalarParameterValue(TEXT("Intensity"),1.5f+5.f*Progress);
  }
 }
}

FString ACultMatchDirector::RelicStatus() const {
 if(!State_||!State_->Moba) return FString();
 const auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(this,0));
 const int32 Index=NearbyRelic(P);
 if(Index==INDEX_NONE) return FString();
 const auto& R=State_->Relics[Index];
 const auto* F=Fighter(P);
 if(!F) return FString();
 if(F->RelicBlockedUntil>State_->Time) return FString::Printf(TEXT("CLAIM INTERRUPTED  |  %.1fs"),F->RelicBlockedUntil-State_->Time);
 if(F->RelicNeedsRelease) return TEXT("RELEASE E TO TRY AGAIN");
 if(R.Contested) return TEXT("RELIC CONTESTED");
 if(F->Level<R.RequiredLevel()) return FString::Printf(TEXT("%s  |  LEVEL %d REQUIRED"),*R.Name,R.RequiredLevel());
 if(R.ChannelSlot==Slot(P)) return FString::Printf(TEXT("CLAIMING %s  |  %.0f%%"),*R.Name,100.f*R.Channel/R.Need);
 return FString::Printf(TEXT("HOLD E  |  %s  |  %.0fs"),*R.Name,R.Need);
}
