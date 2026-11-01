#include "CultVesselSurface.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void CultVesselSurface::Apply(ACultUlhuPlayerPawn* Hero,UStaticMeshComponent* Body,bool Legacy) {
 auto* Material=Legacy?nullptr:LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_VesselSurface"));
 if(!Material) {
  Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
  Legacy=true;
 }
 if(!Material) return;
 auto* Surface=UMaterialInstanceDynamic::Create(Material,Hero); const bool Allied=Hero->GetFaction()==0;
 if(Legacy) Surface->SetVectorParameterValue(TEXT("Color"),Allied?FLinearColor(.025f,.055f,.07f):FLinearColor(.075f,.025f,.04f));
 else {
  Surface->SetVectorParameterValue(TEXT("BodyColor"),Allied?FLinearColor(.045f,.08f,.09f):FLinearColor(.09f,.05f,.055f));
  Surface->SetVectorParameterValue(TEXT("VeinColor"),Allied?FLinearColor(.12f,.36f,.32f):FLinearColor(.45f,.14f,.09f));
 }
 Body->SetMaterial(0,Surface);
}
