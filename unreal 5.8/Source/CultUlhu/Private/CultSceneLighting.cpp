#include "CultSceneLighting.h"
#include "EngineUtils.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Components/SkyLightComponent.h"

ASkyLight* CultSceneLighting::Ambient(UWorld* W,float Intensity,FLinearColor Color) {
 if(!W) return nullptr;
 auto* Cube=LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
 if(!Cube) { UE_LOG(LogTemp,Error,TEXT("[CultLighting] Required runtime ambient cubemap is missing")); return nullptr; }
 ASkyLight* Primary=nullptr;
 for(TActorIterator<ASkyLight> It(W);It;++It) if(IsValid(*It)) {
  if(!Primary) Primary=*It;
  else { auto* C=It->GetLightComponent(); C->SetMobility(EComponentMobility::Movable); C->SetIntensity(0); C->SetVisibility(false); }
 }
 if(!Primary) Primary=W->SpawnActor<ASkyLight>();
 if(!Primary) return nullptr;
 Primary->Tags.AddUnique(TEXT("CultRuntimeAmbient")); Primary->SetActorHiddenInGame(false);
 auto* C=Primary->GetLightComponent(); C->SetMobility(EComponentMobility::Movable); C->SetVisibility(true);
 C->SetRealTimeCaptureEnabled(false); C->SourceType=SLS_SpecifiedCubemap;
 C->SetCubemap(Cube); C->SetIntensity(Intensity); C->SetLightColor(Color);
 C->SetLowerHemisphereColor(FLinearColor(.025f,.035f,.055f));
 return Primary;
}
