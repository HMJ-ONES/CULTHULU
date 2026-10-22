#include "CultPopulation.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);
#include "CultWorldDirector.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void ACultPopulation::CreateHumanAppearance()
{
 Body_->SetRelativeScale3D(FVector(.55,.55,.9)); Body_->SetRelativeLocation(FVector(0,0,-10));
 auto Part=[&](const TCHAR* Name,const TCHAR* Shape,FVector Position,FVector Scale) {
  auto* Mesh=NewObject<UStaticMeshComponent>(this,Name); Mesh->SetupAttachment(Appearance_); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Shape)); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetRelativeLocation(Position); Mesh->SetRelativeScale3D(Scale); Mesh->RegisterComponent(); HumanParts_.Add(Mesh); return Mesh;
 };
 const TCHAR* Sphere=TEXT("/Engine/BasicShapes/Sphere.Sphere"),*Cube=TEXT("/Engine/BasicShapes/Cube.Cube");
 Part(TEXT("Hood"),Sphere,FVector(0,0,56),FVector(.48,.45,.52));
 auto* Face=Part(TEXT("Face"),Sphere,FVector(17,0,57),FVector(.30,.33,.35));
 LeftArm_=Part(TEXT("LeftArm"),Cube,FVector(0,-35,8),FVector(.18,.18,.64)); RightArm_=Part(TEXT("RightArm"),Cube,FVector(0,35,8),FVector(.18,.18,.64));
 LeftLeg_=Part(TEXT("LeftLeg"),Cube,FVector(0,-15,-58),FVector(.20,.20,.62)); RightLeg_=Part(TEXT("RightLeg"),Cube,FVector(0,15,-58),FVector(.20,.20,.62));
 if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))) {
  auto* Skin=UMaterialInstanceDynamic::Create(Material,this); Skin->SetVectorParameterValue(TEXT("Color"),FLinearColor(.48,.37,.30)); Face->SetMaterial(0,Skin);
 }
}
void ACultPopulation::UpdateHumanAppearance(float Dt)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, PopulationAppearance);
 if(!Appearance_) return; RefreshAuthoredHuman(); AnimateAuthoredHuman(Dt); ContributionTime_=FMath::Max(0.f,ContributionTime_-Dt);
 const float Time=GetWorld()->GetTimeSeconds(); bool Walking=GetVelocity().Size2D()>15;
 const bool Prayer=Activity_==TEXT("Pray")||Activity_==TEXT("OmenReading")||Activity_==TEXT("ChantingCircle");
 const bool Rite=Activity_==TEXT("SacrificePerformer")||Activity_==TEXT("Necromancy");
 const bool Preach=Activity_==TEXT("Preach")||Activity_==TEXT("Convert")||Activity_==TEXT("WhisperCampaign");
 const bool Working=Activity_==TEXT("Build")||Activity_==TEXT("Torture");
 const bool Fighting=Activity_==TEXT("Raid")||Activity_==TEXT("Attack")||Activity_==TEXT("WildsHunt");
 const bool Kneeling=(bCaptive_||Prayer||Activity_==TEXT("SacrificeVictim"))&&!Walking;
 FLinearColor FactionColor=Faction_==0?FLinearColor(.06,.36,.28):Faction_==1?FLinearColor(.55,.15,.08):Faction_==2?FLinearColor(.3,.12,.46):FLinearColor(.32,.28,.24);
 if(AppearanceFaction_!=Faction_||AppearanceCultist_!=bCultist_) {
 AppearanceFaction_=Faction_; AppearanceCultist_=bCultist_;
 auto* Robe=LoadObject<UMaterialInterface>(nullptr,bCultist_?TEXT("/Game/CultUlhu/Materials/M_CultistBody"):TEXT("/Game/CultUlhu/Materials/M_CivilianBody"));
 if(Robe) { for(int I=0;I<HumanParts_.Num();++I) if(I!=1) HumanParts_[I]->SetMaterial(0,Robe); }
 // A colored sash makes allegiance visible even without reading the overhead label.
 if(auto* Hood=HumanParts_.IsEmpty()?nullptr:HumanParts_[0]) {
  auto* Tint=Cast<UMaterialInstanceDynamic>(Hood->GetMaterial(0));
  if(!Tint) { Tint=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")),this); Hood->SetMaterial(0,Tint); } Tint->SetVectorParameterValue(TEXT("Color"),FactionColor);
 }
 }
 for(auto* Part:HumanParts_) Part->SetVisibility(!bSummoned_&&!HumanPose_);
 if(!HumanParts_.IsEmpty()) {
  auto* Badge=HumanParts_[0]; Badge->SetRelativeLocation(HumanPose_?FVector(28,0,25):FVector(0,0,56)); Badge->SetRelativeScale3D(HumanPose_?FVector(.14):FVector(.48,.45,.52)); Badge->SetVisibility(!bSummoned_);
 }
 Appearance_->SetRelativeRotation(Activity_==TEXT("Rest")?FRotator(80,0,0):FRotator::ZeroRotator); Appearance_->SetRelativeLocation(FVector(0,0,Kneeling&&!HumanPose_?-26:0));
 Body_->SetVisibility(!HumanPose_);
 if(!HumanPose_) { CSV_SCOPED_TIMING_STAT(CultRuntime, PopulationFallbackPose);
 const float Step=Walking?FMath::Sin(Time*8)*32:0;
 float Arm=Step; if(!Walking&&Prayer) Arm=-80+FMath::Sin(Time*2)*8; if(!Walking&&Rite) Arm=-135+FMath::Sin(Time*1.5f)*12;
 if(!Walking&&Preach) Arm=-55+FMath::Sin(Time*3)*28; if(!Walking&&(Working||Fighting)) Arm=-45+FMath::Sin(Time*7)*65;
 LeftArm_->SetRelativeRotation(FRotator(Prayer||Rite?Arm:-Arm,0,0)); RightArm_->SetRelativeRotation(FRotator(Arm,0,0));
 LeftLeg_->SetRelativeRotation(FRotator(Kneeling?75:-Step,0,0)); RightLeg_->SetRelativeRotation(FRotator(Kneeling?75:Step,0,0));
 }
 // Screen-space inspection is drawn on demand by the HUD, not by every NPC.
}
FString ACultPopulation::InspectionRole() const
{
 if(bCaptive_) return TEXT("Captive");
 if(bLunatic_) return TEXT("Lunatic");
 if(bSorcerer_) return TEXT("Sorcerer");
 return bCultist_?TEXT("Cultist"):bSummoned_?TEXT("Summon"):TEXT("Civilian");
}
FString ACultPopulation::InspectionActivity() const
{
 static const TMap<FString,FString> Names={
  {TEXT("Pray"),TEXT("Praying")},{TEXT("Build"),TEXT("Building")},{TEXT("Convert"),TEXT("Converting")},
  {TEXT("Preach"),TEXT("Preaching")},{TEXT("Raid"),TEXT("Raiding")},{TEXT("Attack"),TEXT("Fighting")},
  {TEXT("Retreat"),TEXT("Retreating")},{TEXT("Rest"),TEXT("Resting")},{TEXT("Patrol"),TEXT("Patrolling")},
  {TEXT("Wander"),TEXT("Wandering")},{TEXT("Follow"),TEXT("Following")},{TEXT("Torture"),TEXT("Torturing")},
  {TEXT("SacrificePerformer"),TEXT("Making sacrifice")},{TEXT("SacrificeVictim"),TEXT("Sacrifice victim")},
  {TEXT("WildsHunt"),TEXT("Hunting")},{TEXT("MendEffigy"),TEXT("Mending effigy")},
  {TEXT("OmenReading"),TEXT("Reading omens")},{TEXT("ChantingCircle"),TEXT("Chanting")},
  {TEXT("DreamSharing"),TEXT("Sharing dreams")},{TEXT("WhisperCampaign"),TEXT("Spreading whispers")}
 };
 const auto* Name=Names.Find(Activity_); FString Out=Name?*Name:Activity_;
 if(bWorking_) Out+=FString::Printf(TEXT(" %.0f%%"),WorkProgress_*100);
 return Out;
}
