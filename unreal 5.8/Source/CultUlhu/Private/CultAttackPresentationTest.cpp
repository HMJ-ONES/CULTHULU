#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "CultSkillAttack.h"
#include "CultMatchUnit.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/GameViewportClient.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

struct FCultAttackPresentationTestAccess {
 static FBox Geometry(ACultSkillAttack* A) {
  FBox B(ForceInit); if(auto* S=A->Telegraph->GetProcMeshSection(0)) for(auto& V:S->ProcVertexBuffer) B+=V.Position; return B;
 }
 static void CommitDirection(ACultSkillAttack* A,FVector D) { A->Direction=D.GetSafeNormal(); A->BuildWarningGeometry(); }
 static FLinearColor Color(ACultSkillAttack* A) { return A->SignalMaterial->K2_GetVectorParameterValue(TEXT("Color")); }
 static void Advance(ACultSkillAttack* A,float Delta) { A->FirstTick=false; A->Tick(Delta); }
 static bool CoreVisible(ACultSkillAttack* A) { return A->Visual->IsVisible(); }
};
class FCultAttackSignalObservation:public IAutomationLatentCommand {
 FDelegateHandle CaptureHandle; bool Captured=false;
 FAutomationTestBase* Test; int Phase=0; float At=0; double Started=FPlatformTime::Seconds();
 FString Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/AttackSignals.png"));
public:
 FCultAttackSignalObservation(FAutomationTestBase* T):Test(T) {}
 ~FCultAttackSignalObservation() { if(CaptureHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>45) { Test->AddError(TEXT("Attack signal verification timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(!W) return false;
  if(Phase==0) {
   TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UUserWidget::StaticClass(),false); for(auto* UI:Widgets) UI->RemoveFromParent();
   auto Unit=[&](int Team,FVector Where) {
    auto* U=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),FTransform(Where),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    U->Team=Team; U->Kind=3; U->FinishSpawning(FTransform(Where)); return U;
   };
   auto* Friendly=Unit(0,FVector(-24000,0,41000)); auto* Hostile=Unit(1,FVector(24000,0,41000));
   auto Attack=[&](ACultSkillAttack::EShape Kind,AActor* Owner,FVector Where) {
    auto* A=W->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),FTransform(Where),Owner,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    A->Shape=Kind; A->Reach=1000; A->Radius=40; A->Windup=5; A->Duration=3; A->Direction=FVector::ForwardVector; A->FinishSpawning(FTransform(Where)); A->SetActorTickEnabled(false); return A;
   };
   using S=ACultSkillAttack::EShape;
   auto* Sphere=Attack(S::Sphere,Friendly,FVector(0,0,41000));
   const auto SB=FCultAttackPresentationTestAccess::Geometry(Sphere);
   Test->TestTrue(TEXT("Aura warning has actual vertical extent as well as horizontal range"),SB.Min.Z<-990&&SB.Max.Z>990&&SB.Max.X>990);
   auto* Disk=Attack(S::GroundDisk,Hostile,FVector(0,0,40000)); const auto DB=FCultAttackPresentationTestAccess::Geometry(Disk);
   Test->TestTrue(TEXT("Ground impact stays planar while matching its collision radius"),DB.GetSize().Z<60&&DB.Max.X>990&&DB.Max.Y>990);
   auto* Cone=Attack(S::Cone,Hostile,FVector(0,0,41000)); const auto CB=FCultAttackPresentationTestAccess::Geometry(Cone);
   Test->TestTrue(TEXT("Cone boundary follows the spherical sector used by collision"),CB.Max.X>700&&CB.Max.X<730&&CB.Max.Y>700&&CB.Max.Z>700);
   for(auto* A:{Sphere,Disk,Cone}) {
    auto* Mesh=A->FindComponentByClass<UProceduralMeshComponent>();
    Test->TestNotNull(TEXT("One continuous outline component exists"),Mesh);
    Test->TestEqual(TEXT("Presentation never adds blocking collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    TArray<UStaticMeshComponent*> Parts; A->GetComponents(Parts); Test->TestTrue(TEXT("Area presentation avoids forty separate marker components"),Parts.Num()<=1);
   }
   const auto FC=FCultAttackPresentationTestAccess::Color(Sphere),EC=FCultAttackPresentationTestAccess::Color(Disk);
   Test->TestTrue(TEXT("Friendly and hostile casts have distinct color families"),FC.B>FC.R&&EC.R>EC.B);
   auto* Tilted=Attack(S::Sweep,Friendly,FVector(0,0,41000));
   FCultAttackPresentationTestAccess::CommitDirection(Tilted,FVector(.8f,0,-.6f));
   const auto TB=FCultAttackPresentationTestAccess::Geometry(Tilted);
   Test->TestTrue(TEXT("A pitched swing warning preserves the committed three-dimensional arc"),TB.Min.Z<-590&&TB.Max.Z<10&&TB.Max.X>790&&TB.Max.X<820); Tilted->Destroy();
   Sphere->Destroy(); Disk->Destroy(); Cone->Destroy();
   // Exercise real callbacks: visual warnings must not cause early contact.
   auto* Target=Unit(1,FVector(600,0,41250)); auto* Contact=Attack(S::GroundDisk,Friendly,FVector(0,0,40000));
   Contact->Windup=.5f; Contact->Duration=.3f; int Contacts=0;
   Contact->Hit=[&](AActor* Actor,FVector){ if(Actor==Target) ++Contacts; };
   FCultAttackPresentationTestAccess::Advance(Contact,.2f); Test->TestEqual(TEXT("Warning phase cannot hit an actor"),Contacts,0);
   FCultAttackPresentationTestAccess::Advance(Contact,.4f); Test->TestEqual(TEXT("The same committed volume contacts only when active"),Contacts,1);
   Contact->Destroy(); Target->Destroy();
   auto* Beam=Attack(S::Beam,Friendly,FVector(0,0,41000));
   Test->TestFalse(TEXT("Beam core stays hidden during its charge"),FCultAttackPresentationTestAccess::CoreVisible(Beam));
   FCultAttackPresentationTestAccess::Advance(Beam,5.1f);
   Test->TestTrue(TEXT("Beam core appears at activation"),FCultAttackPresentationTestAccess::CoreVisible(Beam)); Beam->Destroy();
   auto* Bolt=Attack(S::Bolt,Hostile,FVector(0,3000,41000)); Bolt->Windup=.5f; Bolt->Speed=2000;
   FCultAttackPresentationTestAccess::Advance(Bolt,.6f); const auto BB=FCultAttackPresentationTestAccess::Geometry(Bolt);
   Test->TestTrue(TEXT("Projectile trail follows its actual swept position, behind its leading core"),FMath::IsNearlyEqual(float(BB.Max.X),200.f,5.f)&&BB.Min.X>60&&BB.Min.X<80&&FMath::IsNearlyEqual(float(Bolt->ProjectilePosition().X),200.f,2.f));
   FCultAttackPresentationTestAccess::Advance(Bolt,.05f); const auto Moved=FCultAttackPresentationTestAccess::Geometry(Bolt);
   Test->TestTrue(TEXT("A moving trail updates its existing geometry to the new projectile position"),FMath::IsNearlyEqual(float(Moved.Max.X),300.f,5.f)&&Moved.Min.X>160&&Moved.Min.X<180); Bolt->Destroy();
   // Separate visual fixture: six real attack actors, frozen for inspection.
   auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,39900),FRotator::ZeroRotator); auto* C=Floor->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); C->SetWorldScale3D(FVector(150,100,2));
   C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_ArenaSurface")));
   auto* Light=W->SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-55,-35,0)); Light->GetLightComponent()->SetIntensity(3.f);
   const FVector CameraPoint(-9000,-13000,52000); auto* Camera=W->SpawnActor<ACameraActor>(CameraPoint,(FVector(0,0,40500)-CameraPoint).Rotation());
   auto& Post=Camera->GetCameraComponent()->PostProcessSettings; Camera->GetCameraComponent()->SetFieldOfView(55); Post.bOverride_AutoExposureMethod=true; Post.AutoExposureMethod=AEM_Manual; Post.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Post.AutoExposureApplyPhysicalCameraExposure=false; Post.bOverride_AutoExposureBias=true; Post.AutoExposureBias=.75f;
   W->GetFirstPlayerController()->SetViewTarget(Camera); W->GetFirstPlayerController()->bShowMouseCursor=false;
   const S Shapes[]={S::GroundDisk,S::Sphere,S::Cone,S::Sweep,S::Beam,S::Bolt};
   const TCHAR* Names[]={TEXT("GROUND IMPACT"),TEXT("AURA VOLUME"),TEXT("CONE"),TEXT("SWING"),TEXT("BEAM"),TEXT("PROJECTILE")};
   for(int I=0;I<6;++I) {
    const FVector Origin((I%3-1)*4200,(I/3)*4000-2000,40000+(I==0?0:1100));
    auto* A=Attack(Shapes[I],I%2?Hostile:Friendly,Origin);
    if(I==3) A->Duration=.25f;
    if(I>=3) FCultAttackPresentationTestAccess::Advance(A,5.12f);
    auto* Label=W->SpawnActor<AActor>(FVector(Origin.X,Origin.Y-1500,40050),FRotator::ZeroRotator);
    auto* Text=NewObject<UTextRenderComponent>(Label); Label->SetRootComponent(Text); Text->RegisterComponent(); Text->SetText(FText::FromString(Names[I])); Text->SetWorldSize(220); Text->SetHorizontalAlignment(EHTA_Center); Text->SetWorldLocation(FVector(Origin.X,Origin.Y-1500,40050)); Text->SetWorldRotation((CameraPoint-Text->GetComponentLocation()).Rotation());
   }
   if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
   CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([this](int32 Width,int32 Height,const TArray<FColor>& Bitmap) {
    int Teal=0,Coral=0;
    for(const auto& C:Bitmap) { Teal+=C.B>100&&C.G>80&&C.B>C.R*1.35f&&C.G>C.R*1.2f; Coral+=C.R>120&&C.R>C.G*1.5f&&C.R>C.B*1.5f; }
    Test->TestTrue(TEXT("Rendered image contains visible friendly signals"),Teal>40);
    Test->TestTrue(TEXT("Rendered image contains visible hostile signals"),Coral>40);
    Test->AddInfo(FString::Printf(TEXT("Signal render: %d teal pixels, %d coral pixels"),Teal,Coral));
    TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Width,Height,MakeArrayView(Bitmap),PNG); FFileHelper::SaveArrayToFile(PNG,*Shot); Captured=true;
   });
   At=W->GetTimeSeconds(); Phase=1; return false;
  }
  if(Phase==1&&W->GetTimeSeconds()-At>1) { IFileManager::Get().Delete(*Shot); FScreenshotRequest::RequestScreenshot(Shot,false,false); At=W->GetTimeSeconds(); Phase=2; return false; }
  if(Phase==2&&W->GetTimeSeconds()-At>.5f) { Test->TestTrue(TEXT("Fresh rendered signal fixture saved"),Captured&&FPaths::FileExists(Shot)); UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle); CaptureHandle.Reset(); return true; }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultAttackSignalTest,"CultUlhu.Menu.AttackSignals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultAttackSignalTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5f)); ADD_LATENT_AUTOMATION_COMMAND(FCultAttackSignalObservation(this)); return true; }
#endif
