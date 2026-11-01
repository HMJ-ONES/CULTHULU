#include "CultMatchSurfaces.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
namespace {
TAutoConsoleVariable<int32> StoneEnabled(TEXT("cult.Match.Stone"),1,TEXT("Procedural world-aligned stone on tagged arena surfaces. Zero uses the original flat material."));
TAutoConsoleVariable<int32> DominationFinish(TEXT("cult.Domination.Refinement"),1,TEXT("Original broad Domination stone and architectural relief. Zero retains the previous presentation."));
}
bool CultMatchSurfaces::Enabled() { return StoneEnabled.GetValueOnGameThread()!=0; }
bool CultMatchSurfaces::RefinedDomination() { return DominationFinish.GetValueOnGameThread()!=0; }
UMaterialInterface* CultMatchSurfaces::StoneMaterial(bool Domination) {
 if(Domination&&RefinedDomination()) if(auto* New=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_DominationStone_v1"))) return New;
 return LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_MatchStone"));
}
int CultMatchSurfaces::Apply(UWorld* World,bool Stone,bool Domination) {
 auto* Base=Stone?StoneMaterial(Domination):LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface"));
 if(!World||!Base) return 0;
 int Count=0;
 for(TActorIterator<AActor> It(World);It;++It) if(It->ActorHasTag(TEXT("CultMatchStoneSurface"))) {
  TArray<UMeshComponent*> Meshes; It->GetComponents(Meshes);
  for(auto* Mesh:Meshes) {
  for(int Slot=0;Slot<Mesh->GetNumMaterials();++Slot) {
  auto* Current=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
  const auto Color=Current?Current->K2_GetVectorParameterValue(TEXT("Color")):FLinearColor(.12f,.16f,.18f);
  auto* Mat=UMaterialInstanceDynamic::Create(Base,*It); Mat->SetVectorParameterValue(TEXT("Color"),Color);
  if(Stone) Mat->SetScalarParameterValue(TEXT("Masonry"),It->ActorHasTag(TEXT("CultMatchRockSurface"))?0.f:1.f);
  Mesh->SetMaterial(Slot,Mat);
  }
  ++Count;
  }
 }
 return Count;
}
