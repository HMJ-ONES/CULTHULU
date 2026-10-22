#include "CultMatchSurfaces.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
namespace {
TAutoConsoleVariable<int32> StoneEnabled(TEXT("cult.Match.Stone"),1,TEXT("Procedural world-aligned stone on tagged arena surfaces. Zero uses the original flat material."));
}
bool CultMatchSurfaces::Enabled() { return StoneEnabled.GetValueOnGameThread()!=0; }
int CultMatchSurfaces::Apply(UWorld* World,bool Stone) {
 auto* Base=LoadObject<UMaterialInterface>(nullptr,Stone?TEXT("/Game/CultUlhu/Materials/M_MatchStone"):TEXT("/Game/CultUlhu/Materials/M_ArenaSurface"));
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
