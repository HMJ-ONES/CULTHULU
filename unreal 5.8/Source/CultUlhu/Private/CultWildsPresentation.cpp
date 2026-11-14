#include "CultWildsPresentation.h"
#include "CultSceneLighting.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "CultLivingLandscape.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

namespace {
TAutoConsoleVariable<int32> Enabled(TEXT("cult.Wilds.Presentation"),1,TEXT("Use the Free Roam moonlight/exposure profile. Zero preserves the old level lighting for comparison."));
}

void CultWildsPresentation::Apply(UWorld* World)
{
 if(!World||!Enabled.GetValueOnGameThread()) return;
 // One high-priority volume controls adaptation across bright courtyards and caves.
 APostProcessVolume* Grade=nullptr;
 for(TActorIterator<APostProcessVolume> It(World);It;++It) if(It->ActorHasTag(TEXT("CultWildsGrade"))) Grade=*It;
 if(!Grade) { Grade=World->SpawnActor<APostProcessVolume>(); Grade->Tags.Add(TEXT("CultWildsGrade")); }
 Grade->bUnbound=true; Grade->Priority=100;
 auto& Settings=Grade->Settings;
 Settings.bOverride_AutoExposureMethod=true; Settings.AutoExposureMethod=AEM_Manual;
 Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Settings.AutoExposureApplyPhysicalCameraExposure=false;
 Settings.bOverride_AutoExposureBias=true; Settings.AutoExposureBias=1.f;
 // Soft image-based fill keeps enemies readable under roofs; direct moonlight
 // still provides the stronger directional shadows outside.
 Settings.AmbientCubemap=LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
 Settings.bOverride_AmbientCubemapIntensity=true; Settings.AmbientCubemapIntensity=.3f;
 Settings.bOverride_AmbientCubemapTint=true; Settings.AmbientCubemapTint=FLinearColor(.6f,.73f,.78f);
 Settings.bOverride_BloomIntensity=true; Settings.BloomIntensity=.22f;
 Settings.bOverride_BloomThreshold=true; Settings.BloomThreshold=1.f;
 Settings.bOverride_VignetteIntensity=true; Settings.VignetteIntensity=.15f;

 bool HasMoon=false;
 for(TActorIterator<ADirectionalLight> It(World);It;++It) {
  auto* Light=It->GetLightComponent(); Light->SetMobility(EComponentMobility::Movable); Light->SetIntensity(HasMoon?0.f:2.5f);
  Light->SetLightColor(FLinearColor(.62f,.76f,.82f));
  if(!HasMoon) It->SetActorRotation(FRotator(-32,-35,0));
  HasMoon=true;
 }
 CultSceneLighting::Ambient(World,.55f,FLinearColor(.68f,.78f,.79f));
 // Do not let the atmosphere's daytime sun disk contradict the moonlit rig.
 for(TActorIterator<AActor> It(World);It;++It) {
  TArray<USkyAtmosphereComponent*> Atmospheres; It->GetComponents(Atmospheres);
  for(auto* Atmosphere:Atmospheres) Atmosphere->SetVisibility(false);
 }
 bool HasDome=false;
 for(TActorIterator<AStaticMeshActor> It(World);It;++It) if(It->ActorHasTag(TEXT("CultWildsSky"))) HasDome=true;
 if(!HasDome) {
  auto* Dome=World->SpawnActor<AStaticMeshActor>(); Dome->Tags.Add(TEXT("CultWildsSky")); Dome->SetActorEnableCollision(false);
  auto* Mesh=Dome->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
  Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_WildsSky.M_WildsSky")));
  Mesh->SetWorldScale3D(FVector(10000)); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  // Basic shapes may have an imported pivot offset. Center the actual sphere,
  // rather than assuming its actor origin encloses the whole generated world.
  if(Mesh->GetStaticMesh()) Dome->SetActorLocation(-Mesh->GetStaticMesh()->GetBoundingBox().GetCenter()*10000);
  Mesh->SetCastShadow(false); Mesh->SetReceivesDecals(false); Mesh->SetForceDisableNanite(true);
 }
 bool HasCaveFill=false;
 for(TActorIterator<APointLight> It(World);It;++It) if(It->ActorHasTag(TEXT("CultCaveFill"))) HasCaveFill=true;
 if(!HasCaveFill) for(const FVector& Site:ACultLivingLandscape::CaveSites()) for(float Depth:{-3500.f,3500.f}) {
  auto* Fill=World->SpawnActor<APointLight>(Site+FVector(0,Depth,3500),FRotator::ZeroRotator);
  Fill->Tags.Add(TEXT("CultCaveFill")); auto* Light=Cast<UPointLightComponent>(Fill->GetLightComponent());
  Light->SetMobility(EComponentMobility::Movable); Light->SetCastShadows(false);
  Light->SetIntensity(1200000); Light->SetLightColor(FLinearColor(.48f,.64f,.58f)); Light->SetAttenuationRadius(10000);
  Light->MaxDrawDistance=25000; Light->MaxDistanceFadeRange=5000;
 }
 auto ConfigureFog=[](UExponentialHeightFogComponent* Fog) {
  Fog->SetFogDensity(.004f); Fog->SetFogHeightFalloff(.06f);
  Fog->SetStartDistance(5000); Fog->SetFogMaxOpacity(.8f);
  Fog->SetFogInscatteringColor(FLinearColor(.018f,.026f,.034f));
  Fog->SetVolumetricFog(false);
 };
 bool HasFog=false;
 for(TActorIterator<AExponentialHeightFog> It(World);It;++It) { ConfigureFog(It->GetComponent()); HasFog=true; }
 if(!HasFog) ConfigureFog(World->SpawnActor<AExponentialHeightFog>()->GetComponent());
}
