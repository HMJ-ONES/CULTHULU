#include "CultWardArchitecture.h"
#include "CultMatchUnit.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"

namespace {
TAutoConsoleVariable<int32> WardArt(TEXT("cult.Match.WardArt"),1,TEXT("Assembled stone lane/nexus wards. Zero uses the legacy blockout at spawn."));
const FName ArtTag(TEXT("CultWardArchitecture"));
}
bool CultWardArchitecture::Enabled() { return WardArt.GetValueOnGameThread()!=0; }
UStaticMeshComponent* CultWardArchitecture::Create(ACultMatchUnit* Unit,bool Force) {
 if(!Unit||(Unit->Kind!=3&&Unit->Kind!=5)||(!Force&&!Enabled())) return nullptr;
 auto* Asset=LoadObject<UStaticMesh>(nullptr,Unit->Kind==5?TEXT("/Game/CultUlhu/World/Wards/SM_NexusWard.SM_NexusWard"):TEXT("/Game/CultUlhu/World/Wards/SM_LaneWard.SM_LaneWard"));
 if(!Asset) return nullptr;
 auto* Body=NewObject<UStaticMeshComponent>(Unit,TEXT("WardArchitecturalBody"));
 Body->SetupAttachment(Unit->GetRootComponent()); Body->SetMobility(EComponentMobility::Movable); Body->SetStaticMesh(Asset); Body->SetForceDisableNanite(true);
 Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Body->ComponentTags.Add(ArtTag);
 for(int I=0;I<Asset->GetStaticMaterials().Num();++I) {
  auto* Base=Asset->GetMaterial(I); if(!Base) continue;
  auto* Mat=UMaterialInstanceDynamic::Create(Base,Unit); const auto Name=Base->GetName();
  if(Name==TEXT("M_AttackSignal")) { Mat->SetVectorParameterValue(TEXT("Color"),Unit->Team==0?FLinearColor(.24f,.73f,.64f):FLinearColor(.91f,.32f,.25f)); Mat->SetScalarParameterValue(TEXT("Intensity"),.65f); }
  else { Mat->SetVectorParameterValue(TEXT("Color"),Name==TEXT("M_MatchStone")?FLinearColor(.14f,.18f,.19f):FLinearColor(.32f,.37f,.33f)); Mat->SetScalarParameterValue(TEXT("Masonry"),1); }
  Body->SetMaterial(I,Mat);
 }
 Unit->AddInstanceComponent(Body); Body->RegisterComponent(); return Body;
}
void CultWardArchitecture::Extinguish(ACultMatchUnit* Unit) {
 TArray<UStaticMeshComponent*> Parts; Unit->GetComponents(Parts);
 for(auto* Part:Parts) if(Part->ComponentHasTag(ArtTag)) for(int I=0;I<Part->GetNumMaterials();++I) {
  if(Part->GetStaticMesh()->GetMaterial(I)->GetName()==TEXT("M_AttackSignal")) if(auto* Mat=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(I))) Mat->SetScalarParameterValue(TEXT("Intensity"),0);
 }
}
#if WITH_EDITOR
void CultWardArchitecture::Compare(ACultMatchUnit* Unit,bool Refined) {
 if(!Unit||(Unit->Kind!=3&&Unit->Kind!=5)) return;
 TArray<UStaticMeshComponent*> Parts; Unit->GetComponents(Parts);
 UStaticMeshComponent* Body=nullptr; for(auto* Part:Parts) if(Part->ComponentHasTag(ArtTag)) Body=Part;
 if(Refined&&!Body) Body=Create(Unit,true);
 Unit->GetComponents(Parts);
 for(auto* Part:Parts) Part->SetVisibility(Part==Body?Refined:!Refined);
}
#endif
