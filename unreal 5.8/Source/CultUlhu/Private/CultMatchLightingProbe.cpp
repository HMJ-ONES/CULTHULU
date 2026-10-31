#if WITH_DEV_AUTOMATION_TESTS
#include "CultMatchLightingProbe.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

void FCultMatchLightingProbe::Restore() {
 if(auto* PC=Controller.Get()) { if(Original.IsValid()) PC->SetViewTarget(Original.Get()); if(auto* HUD=PC->GetHUD()) HUD->bShowHUD=ShowHUD; }
 if(Camera.IsValid()) Camera->Destroy(); Camera.Reset();
 for(auto& Actor:Fixtures) if(Actor.IsValid()) Actor->Destroy(); Fixtures.Reset();
}
FCultMatchLightingProbe::~FCultMatchLightingProbe() { Restore(); }
bool FCultMatchLightingProbe::Tick(UWorld* W,bool Host,int Mode) {
 if(Done) return true;
 auto* PC=W->GetFirstPlayerController(); if(!PC||!PC->PlayerCameraManager) return false;
 const float Now=W->GetTimeSeconds();
 if(Phase==0) {
  Prefix=FPaths::ProjectSavedDir()/TEXT("Automation")/FString::Printf(TEXT("Room%d_lighting_%s"),Mode,Host?TEXT("host"):TEXT("guest"));
  Controller=PC; Original=PC->GetViewTarget(); if(auto* HUD=PC->GetHUD()) { ShowHUD=HUD->bShowHUD; HUD->bShowHUD=false; }
  Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetConstraintAspectRatio(false); Camera->GetCameraComponent()->SetFieldOfView(75); Camera->GetCameraComponent()->PostProcessBlendWeight=0;
  PC->SetViewTarget(Camera.Get());
  for(int Side:{-1,1}) {
   auto* A=W->SpawnActor<AStaticMeshActor>(FVector((Mode?0:12000)+Side*1200,0,1500),FRotator::ZeroRotator);
   auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Side<0?TEXT("/Engine/BasicShapes/Sphere"):TEXT("/Engine/BasicShapes/Cube")));
   C->SetWorldScale3D(FVector(12)); C->SetCollisionProfileName(TEXT("NoCollision")); A->SetActorEnableCollision(false);
   auto* M=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface")),A); M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.18f,.18f,.18f)); C->SetMaterial(0,M); Fixtures.Add(A);
  }
  TArray<FString> Lights,Skies,Grades;
  for(TActorIterator<ADirectionalLight> It(W);It;++It) { const auto* C=It->GetLightComponent(); Lights.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"intensity\":%.4f,\"visible\":%s,\"hidden\":%s,\"pitch\":%.3f,\"yaw\":%.3f}"),*It->GetName(),C->Intensity,C->IsVisible()?TEXT("true"):TEXT("false"),It->IsHidden()?TEXT("true"):TEXT("false"),It->GetActorRotation().Pitch,It->GetActorRotation().Yaw)); }
  for(TActorIterator<ASkyLight> It(W);It;++It) { const auto* C=It->GetLightComponent(); Skies.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"intensity\":%.4f,\"sourceType\":%d,\"cubemap\":\"%s\",\"visible\":%s,\"hidden\":%s}"),*It->GetName(),C->Intensity,int(C->SourceType),*GetPathNameSafe(C->Cubemap.Get()),C->IsVisible()?TEXT("true"):TEXT("false"),It->IsHidden()?TEXT("true"):TEXT("false"))); }
  for(TActorIterator<APostProcessVolume> It(W);It;++It) if(It->bUnbound) Grades.Add(FString::Printf(TEXT("{\"priority\":%.1f,\"manualExposure\":%s,\"exposureBias\":%.3f}"),It->Priority,It->Settings.AutoExposureMethod==AEM_Manual?TEXT("true"):TEXT("false"),It->Settings.AutoExposureBias));
  FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"mode\":%d,\"role\":\"%s\",\"fov\":75,\"directional\":[%s],\"sky\":[%s],\"unboundGrades\":[%s]}"),Mode,Host?TEXT("host"):TEXT("guest"),*FString::Join(Lights,TEXT(",")),*FString::Join(Skies,TEXT(",")),*FString::Join(Grades,TEXT(","))),*(Prefix+TEXT(".json")));
  Phase=1;
 }
 if(Phase==1) {
  const FVector Site(Mode?0:12000,View?12500:-12500,6500); Camera->SetActorLocation(Site); Camera->SetActorRotation((FVector(Mode?0:12000,0,1500)-Site).Rotation()); PC->PlayerCameraManager->UpdateCamera(0);
  At=Now; Phase=2; return false;
 }
 if(Phase==2&&Now-At>2) { FScreenshotRequest::RequestScreenshot(Prefix+FString::Printf(TEXT("_%d.png"),View),false,false); At=Now; Phase=3; return false; }
 if(Phase==3&&Now-At>.3f&&IFileManager::Get().FileExists(*(Prefix+FString::Printf(TEXT("_%d.png"),View)))) {
  if(++View<2) { Phase=1; return false; }
  Restore(); FFileHelper::SaveStringToFile(TEXT("ready"),*Prefix); Done=true; return true;
 }
 return false;
}
#endif
