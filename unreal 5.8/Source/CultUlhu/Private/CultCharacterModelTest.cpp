#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultCharacterModel.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "UI/CultCharacterPreviewWidget.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCorpse.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/OutputDeviceFile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

struct FCultCharacterModelTestAccess {
 static ACultCharacterModel* Model(UCultCharacterPreviewWidget* Widget) { return Widget->Model_; }
 static void Capture(UCultCharacterPreviewWidget* Widget,const FString& Id,FAutomationTestBase* Test) {
  if(!Widget->Capture_||!Widget->Target_) { Test->AddError(TEXT("No native model preview target for ")+Id); return; }
  Widget->Capture_->CaptureScene(); FlushRenderingCommands(); TArray<FColor> Pixels;
  Test->TestTrue(TEXT("Native approved-model image can be read"),Widget->Target_->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
  int VisiblePixels=0;
  for(const auto& Pixel:Pixels) if(FMath::Max3(Pixel.R,Pixel.G,Pixel.B)>12) ++VisiblePixels;
  Test->TestTrue(TEXT("Approved-model preview contains visible rendered art: ")+Id,VisiblePixels>512);
  TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(512,256,MakeArrayView(Pixels),PNG);
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Screenshots/ApprovedCharacters"); IFileManager::Get().MakeDirectory(*Directory,true);
  Test->TestTrue(TEXT("Native approved-model image saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/Id+TEXT(".png"))));
 }
};
class FCultApprovedModels:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Index=0,Frames=0,Phase=0,MotionIndex=0; double Started=FPlatformTime::Seconds();
 TStrongObjectPtr<UCultCharacterPreviewWidget> Preview;
 TArray<FString> Motions={TEXT("walk"),TEXT("run"),TEXT("jump"),TEXT("attack"),TEXT("cast"),TEXT("dash"),TEXT("land")};
 TArray<FString> Ids={TEXT("cthulhu_avatar"),TEXT("dagon"),TEXT("mi_go"),TEXT("nyarlathotep"),TEXT("shoggoths"),TEXT("yig"),TEXT("yog_sothoth"),TEXT("nodens"),TEXT("sghllor")};
public:
 explicit FCultApprovedModels(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>240) { Test->AddError(TEXT("Approved model verification timed out")); return true; }
  auto* World=GEditor->PlayWorld.Get(); if(!World||World->bIsTearingDown) return false;
  if(Phase==4) {
   auto* Model=FCultCharacterModelTestAccess::Model(Preview.Get()); if(!Model) { Test->AddError(TEXT("Lost motion preview")); return true; }
   if(++Frames<12) return false;
   if(Frames==12) {
    for(UMeshComponent* Part:Model->Parts()) if(auto* S=Cast<USkeletalMeshComponent>(Part)) { S->SetPosition(.2f,false); if(auto* Player=S->GetSingleNodeInstance()) Player->SetPlaying(false); }
    return false; // Evaluate and render the held pose in a real subsequent frame.
   }
   FCultCharacterModelTestAccess::Capture(Preview.Get(),Ids[Index]+TEXT("_")+Motions[MotionIndex],Test);
   Frames=0; if(++MotionIndex<Motions.Num()) { Model->PreviewMotion(Motions[MotionIndex]); return false; }
   Phase=0; ++Index; return false;
  }
  if(Phase==0) {
   if(!World->GetName().Contains(TEXT("L_MainMenu"))) return false;
   if(!Preview.IsValid()) { Preview.Reset(CreateWidget<UCultCharacterPreviewWidget>(World,UCultCharacterPreviewWidget::StaticClass())); Preview->TakeWidget(); }
   if(Frames==0) { Preview->ShowCharacter(Ids[Index]); if(Ids[Index]==TEXT("mi_go")) FCultCharacterModelTestAccess::Capture(Preview.Get(),TEXT("mi_go_first"),Test); }
   if(++Frames<25) return false;
   auto* Model=FCultCharacterModelTestAccess::Model(Preview.Get()); Test->TestNotNull(TEXT("Approved exact-ID model appears in selection: ")+Ids[Index],Model);
   if(Model) { Test->TestTrue(TEXT("Imported source has visible components"),Model->Parts().Num()>0); FCultCharacterModelTestAccess::Capture(Preview.Get(),Ids[Index],Test); }
   if(Model) for(UMeshComponent* Part:Model->Parts()) {
    Test->AddInfo(FString::Printf(TEXT("MODEL_GEOMETRY %s %s bounds=%s origin=%s scale=%s"),*Ids[Index],*Part->GetName(),*Part->Bounds.BoxExtent.ToString(),*Part->Bounds.Origin.ToString(),*Part->GetRelativeScale3D().ToString()));
    if(auto* Skel=Cast<USkeletalMeshComponent>(Part)) Test->AddInfo(FString::Printf(TEXT("MODEL_RIG %s asset=%s bone0=%s"),*Ids[Index],*Skel->GetSkeletalMeshAsset()->GetPathName(),*Skel->GetBoneTransform(0).ToHumanReadableString()));
    if(auto* Skel=Cast<USkeletalMeshComponent>(Part)) {
     const auto* Data=Skel->GetSkeletalMeshAsset()->GetResourceForRendering(); uint32 Triangles=0;
     if(Data&&Data->LODRenderData.Num()) for(const auto& Section:Data->LODRenderData[0].RenderSections) Triangles+=Section.NumTriangles;
     Test->AddInfo(FString::Printf(TEXT("MODEL_SKELETAL_TRIANGLES %s %u LOD0"),*Ids[Index],Triangles));
    }
    if(auto* Skel=Cast<USkeletalMeshComponent>(Part);Skel&&Ids[Index]==TEXT("mi_go")) {
     TArray<FFinalSkinVertex> Vertices; Skel->GetCPUSkinnedVertices(Vertices,0); FBox Actual(ForceInit);
     for(const auto& Vertex:Vertices) Actual+=Model->GetActorTransform().InverseTransformPosition(Skel->GetComponentTransform().TransformPosition(FVector(Vertex.Position)));
     Test->AddInfo(TEXT("MODEL_ACTUAL mi_go ")+Actual.ToString()+TEXT(" part=")+Skel->GetRelativeLocation().ToString());
     Test->TestTrue(TEXT("Supplied Mi-Go motion stays near the pawn and ground"),Actual.Min.Z>-25&&Actual.Max.Z<230&&Actual.GetCenter().Size2D()<125);
    }
   }
   if(Model&&(Ids[Index]==TEXT("mi_go")||Ids[Index]==TEXT("yig"))) {
    MotionIndex=0; Frames=0; Phase=4; Model->PreviewMotion(Motions[MotionIndex]); return false;
   }
   Frames=0;
   if(++Index<Ids.Num()) return false;
   Preview->NativeDestruct(); Preview.Reset();
   auto* GI=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); GI->SelectedMode=ECultPlayMode::Domination; GI->StartPlay(); Phase=1; Index=0; return false;
  }
  if(!World->GetName().Contains(TEXT("L_DarkWilds"))) return false;
  auto* PC=World->GetFirstPlayerController(); if(!PC||!PC->GetPawn()) return false;
  const auto& Roster=UCultUlhuGameInstance::Roster(); const FString Id=Ids[Index];
  Cast<UCultUlhuGameInstance>(World->GetGameInstance())->SelectedCharacterId=Id;
  auto* Hero=World->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),FTransform(PC->GetPawn()->GetActorLocation()+FVector(0,0,1500)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  UGameplayStatics::FinishSpawningActor(Hero,Hero->GetActorTransform()); Hero->SetActorTickEnabled(false);
  TArray<AActor*> Attached; Hero->GetAttachedActors(Attached); ACultCharacterModel* Model=nullptr;
  for(auto* Actor:Attached) if(auto* Candidate=Cast<ACultCharacterModel>(Actor)) Model=Candidate;
  Test->TestNotNull(TEXT("Canonical pawn installs selected model: ")+Id,Model);
  if(Model) {
   float HeightM=0; for(const auto& E:Roster) if(E.Id==Id) HeightM=E.HeightM;
   Test->TestTrue(TEXT("Canonical character height is scaled once"),FMath::IsNearlyEqual(Hero->GetActorScale3D().Z,HeightM/1.8f,.001f));
   Test->TestTrue(TEXT("Gameplay capsule still blocks"),Hero->GetCapsuleComponent()->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
   Test->TestTrue(TEXT("Movement remains enabled"),Hero->GetCharacterMovement()->MovementMode!=MOVE_None);
   for(UMeshComponent* Part:Model->Parts()) Test->TestEqual(TEXT("Imported art adds no gameplay collision"),Part->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
   const auto Transform=Hero->GetActorTransform(); Hero->ToggleCamera(); Model->Tick(0);
   Test->TestFalse(TEXT("First person keeps the imported body and attack motion visible"),Model->IsHidden());
   Hero->ToggleCamera(); Model->Tick(0); Test->TestFalse(TEXT("Third-person view restores the imported body"),Model->IsHidden());
   TArray<UMeshComponent*> PreviousParts; Hero->GetComponents(PreviousParts);
   for(auto* Part:PreviousParts) if(Part->ComponentHasTag(TEXT("CultReplacedVessel"))) Test->TestFalse(TEXT("Camera toggle never restores the replaced placeholder"),Part->IsVisible());
   Test->TestTrue(TEXT("View switching preserves pawn placement and scale"),Hero->GetActorTransform().Equals(Transform));
   auto* Corpse=ACultCorpse::Leave(Hero); Test->TestNotNull(TEXT("Imported body leaves a persistent corpse"),Corpse);
   if(Corpse) { TArray<UMeshComponent*> Copies; Corpse->GetComponents(Copies); Test->TestTrue(TEXT("Corpse retains actual model geometry"),Copies.Num()>=Model->Parts().Num()); Corpse->Destroy(); }
   Model->Destroy();
  }
  Hero->Destroy(); return ++Index==Ids.Num();
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultApprovedModelTest,"CultUlhu.Art.ApprovedCharacterModels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultApprovedModelTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultApprovedModels(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }

class FReviewedMotionCheck:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Character=0,Role=0,Phase=0,Wait=0;
 double Began=FPlatformTime::Seconds();
 TStrongObjectPtr<UCultCharacterPreviewWidget> Preview;
 TArray<FString> Ids={TEXT("mi_go"),TEXT("shoggoths"),TEXT("dagon"),TEXT("nyarlathotep"),TEXT("yog_sothoth"),TEXT("cthulhu_avatar"),TEXT("yig"),TEXT("sghllor")};
 TArray<FString> Roles={TEXT("idle"),TEXT("walk"),TEXT("run"),TEXT("melee_1"),TEXT("melee_2"),TEXT("melee_3"),TEXT("jump"),TEXT("dash"),TEXT("taunt"),TEXT("land")};
 TArray<FVector> Before;TSet<FString> MeleeAssets;
 void Position(ACultCharacterModel* Model,float Fraction) {
  for(UMeshComponent* Part:Model->Parts()) if(auto* S=Cast<USkeletalMeshComponent>(Part)) if(auto* P=S->GetSingleNodeInstance()) if(auto* Clip=Cast<UAnimSequence>(P->GetCurrentAsset())) {
   P->SetPosition(Clip->GetPlayLength()*Fraction,false);P->SetPlaying(false);S->TickAnimation(0,false);S->RefreshBoneTransforms();
  }
 }
 TArray<FVector> Pose(ACultCharacterModel* Model) {
  TArray<FVector> Values;
  for(UMeshComponent* Part:Model->Parts()) if(auto* S=Cast<USkeletalMeshComponent>(Part)) for(int I=0;I<S->GetNumBones();++I) Values.Add(S->GetBoneTransform(I).GetLocation());
  else { Values.Add(Part->GetRelativeLocation());Values.Add(Part->GetRelativeScale3D()*100);Values.Add(Part->GetRelativeRotation().Euler()); }
  return Values;
 }
public:
 explicit FReviewedMotionCheck(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>360) { Test->AddError(TEXT("Reviewed motion test timed out"));return true; }
  auto* W=GEditor->PlayWorld.Get();if(!W||W->bIsTearingDown||!W->GetName().Contains(TEXT("L_MainMenu"))) return false;
  if(!Preview.IsValid()) {Preview.Reset(CreateWidget<UCultCharacterPreviewWidget>(W,UCultCharacterPreviewWidget::StaticClass()));Preview->TakeWidget();}
  if(Phase==0) {
   Roles.SetNum(10);if(Ids[Character]==TEXT("cthulhu_avatar")) for(int I=1;I<=3;++I) Roles.Add(FString::Printf(TEXT("melee_back_%d"),I));
   Preview->ShowCharacter(Ids[Character]);Role=0;Wait=0;MeleeAssets.Reset();Phase=1;return false;
  }
  auto* Model=FCultCharacterModelTestAccess::Model(Preview.Get());if(!Model) {Test->AddError(TEXT("Missing reviewed model: ")+Ids[Character]);return true;}
  if(Phase==1) {
   Model->SetActorTickEnabled(false);Model->PreviewMotion(Roles[Role]);Wait=0;Phase=2;return false;
  }
  if(Phase==2) {
   if(++Wait<3)return false;
   for(UMeshComponent* Part:Model->Parts()) {
    Test->TestTrue(TEXT("Source materials remain assigned"),Part->GetNumMaterials()>0&&Part->GetMaterial(0)!=nullptr);
    if(auto* S=Cast<USkeletalMeshComponent>(Part)) {
     auto* P=S->GetSingleNodeInstance();auto* Clip=P?Cast<UAnimSequence>(P->GetCurrentAsset()):nullptr;
     Test->TestNotNull(TEXT("Actual motion asset plays: ")+Ids[Character]+TEXT("/")+Roles[Role],Clip);
     if(Clip&&Roles[Role].StartsWith(TEXT("melee_"))) { Test->TestTrue(TEXT("Correct combo clip selected"),Clip->GetName().Contains(Roles[Role]));MeleeAssets.Add(Clip->GetPathName()); }
     if(Clip&&Ids[Character]==TEXT("mi_go")&&Roles[Role]==TEXT("dash")) Test->TestEqual(TEXT("Mi-Go keeps supplied Prowl"),Clip->GetName(),FString(TEXT("MiGo_Anim")));
    }
   }
   Position(Model,.18f);if(Ids[Character]==TEXT("sghllor")) Model->Tick(.04f);Wait=0;Phase=3;return false;
  }
  if(Phase==3) {
   Before=Pose(Model);Position(Model,.63f);if(Ids[Character]==TEXT("sghllor")) Model->Tick(.10f);Wait=0;Phase=4;return false;
  }
  if(++Wait<2)return false;
  const auto After=Pose(Model);float Travel=0;
  if(After.Num()==Before.Num())for(int I=0;I<After.Num();++I)Travel=FMath::Max(Travel,float(FVector::Dist(After[I],Before[I])));
  Test->TestTrue(TEXT("Pose visibly changes: ")+Ids[Character]+TEXT("/")+Roles[Role],Travel>.005f);
  FCultCharacterModelTestAccess::Capture(Preview.Get(),TEXT("reviewed_")+Ids[Character]+TEXT("_")+Roles[Role],Test);
  if(++Role<Roles.Num()) {Phase=1;return false;}
  if(Ids[Character]!=TEXT("sghllor")) Test->TestTrue(TEXT("Three distinct combo assets are installed"),MeleeAssets.Num()>=3);
  if(++Character==Ids.Num())return true;
  Phase=0;return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultReviewedMotionTest,"CultUlhu.Art.ReviewedCharacterMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultReviewedMotionTest::RunTest(const FString&) {ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE());ADD_LATENT_AUTOMATION_COMMAND(FReviewedMotionCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif
