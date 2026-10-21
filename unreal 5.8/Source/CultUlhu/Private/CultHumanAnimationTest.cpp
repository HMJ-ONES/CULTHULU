#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultHumanAnimation.h"
#include "CultPopulation.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayGameMode.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif

namespace {
TArray<FString> HumanSkins() {
 TArray<FString> Out;
#define CULT_HUMAN_SKIN(Id) Out.Add(TEXT(Id));
#define CULT_HUMAN_CLIP(Id)
#include "CultHumanAnimations.inl"
#undef CULT_HUMAN_SKIN
#undef CULT_HUMAN_CLIP
 return Out;
}
TArray<FString> HumanClips() {
 TArray<FString> Out;
#define CULT_HUMAN_SKIN(Id)
#define CULT_HUMAN_CLIP(Id) Out.Add(TEXT(Id));
#include "CultHumanAnimations.inl"
#undef CULT_HUMAN_SKIN
#undef CULT_HUMAN_CLIP
 return Out;
}
bool PoseMatchesOriginal(UPoseableMeshComponent* Leader,const cultulhu::AnimationClip& Clip,const TArray<float>& Times,uint32 PersonId)
{
 if(!Leader) return false; auto* Rig=Cast<USkeletalMesh>(Leader->GetSkinnedAsset()); if(!Rig) return false;
 const auto& Rest=Rig->GetRefSkeleton().GetRefBonePose();
 for(float Time:Times) {
  TArray<FTransform> Expected=Rest;
  const auto Pose=Clip.sampleAt(FMath::Fmod(Time+PersonId*.031,Clip.durationSeconds));
  for(const auto& Bone:Pose) {
   FString Name=UTF8_TO_TCHAR(Bone.first.c_str()); int Index=Rig->GetRefSkeleton().FindBoneIndex(FName(*Name));
   if(Index==INDEX_NONE) { Name.ReplaceInline(TEXT("."),TEXT("_")); Index=Rig->GetRefSkeleton().FindBoneIndex(FName(*Name)); }
   if(!Expected.IsValidIndex(Index)) continue;
   const auto& Rotation=Bone.second.rotEuler; const auto& Position=Bone.second.pos;
   Expected[Index].SetRotation((Rest[Index].GetRotation()*FQuat::MakeFromEuler(FVector(Rotation.x,Rotation.y,Rotation.z))).GetNormalized());
   Expected[Index].AddToTranslation(FVector(Position.x,Position.z,Position.y)*100);
  }
  bool Matches=Expected.Num()==Leader->BoneSpaceTransforms.Num();
  for(int B=0;Matches&&B<Expected.Num();++B) Matches=Expected[B].Equals(Leader->BoneSpaceTransforms[B],.001f);
  if(Matches) return true;
 }
 return false;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHumanAnimationFilesTest,"CultUlhu.Runtime.HumanAnimationFiles",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultHumanAnimationFilesTest::RunTest(const FString&)
{
 auto* Staged=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Animation.UseStagedClips")); const int Before=Staged->GetInt(); Staged->Set(1,ECVF_SetByCode);
 TestEqual(TEXT("Three native human skins are staged"),HumanSkins().Num(),3); TestEqual(TEXT("Every original human action clip is staged"),HumanClips().Num(),16);
#if !WITH_EDITOR
 TestFalse(TEXT("Standalone verification has no adjacent repository animation tree"),FPaths::DirectoryExists(FPaths::ProjectDir()/TEXT("../assets/characters")));
#endif
 FString Report=TEXT("skin,clip,tracks,matching_bones,duration,staged_path\n"); int Loaded=0;
 for(const auto& Skin:HumanSkins()) {
  const bool Mage=Skin==TEXT("cultist_magus"); const FString Prefix=Mage?TEXT("Mage_"):TEXT("Rogue_");
  const FString Folder=TEXT("/Game/CultUlhu/People/")+Skin+TEXT("/")+Skin;
  USkeletalMesh* Body=nullptr;
  for(const FString Part:{FString(TEXT("Body")),Skin==TEXT("cultist_hooded")?FString(TEXT("Head_Hooded")):FString(TEXT("Head")),FString(TEXT("ArmLeft")),FString(TEXT("ArmRight")),FString(TEXT("LegLeft")),FString(TEXT("LegRight"))}) {
   const auto Name=Prefix+Part; auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(Folder/TEXT("SkeletalMeshes")/Name+TEXT(".")+Name));
   TestNotNull(Skin+TEXT(" cooked body part ")+Part,Mesh); if(Part==TEXT("Body")) Body=Mesh;
  }
  for(const auto& Name:Mage?TArray<FString>{TEXT("Mage_Cape"),TEXT("Mage_Hat"),TEXT("2H_Staff")}:TArray<FString>{TEXT("Rogue_Cape")}) TestNotNull(Skin+TEXT(" cooked accessory ")+Name,LoadObject<UStaticMesh>(nullptr,*(Folder/TEXT("StaticMeshes")/Name+TEXT(".")+Name)));
  for(const auto& Clip:HumanClips()) {
   const FString File=CultHumanAnimation::Path(Skin,Clip);
   TestTrue(TEXT("Runtime resolution stays inside staged Content"),File.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("CharacterAnimations"))));
   const auto& Animation=CultHumanAnimation::Find(Skin,Clip);
   TestTrue(Skin+TEXT("/")+Clip+TEXT(" contains usable original keyframes"),Animation.hasTrackData()&&Animation.durationSeconds>0);
   int Matching=0; if(Body) for(const auto& Track:Animation.tracks) { FString Bone=UTF8_TO_TCHAR(Track.first.c_str()); Bone.ReplaceInline(TEXT("."),TEXT("_")); Matching+=Body->GetRefSkeleton().FindBoneIndex(FName(*Bone))!=INDEX_NONE; }
   TestTrue(Skin+TEXT("/")+Clip+TEXT(" samples bones on its actual imported rig"),Matching>0&&!Animation.sampleAt(Animation.durationSeconds*.37).empty());
#if WITH_EDITOR
   TArray<uint8> Original,StagedBytes; const auto Source=FPaths::ProjectDir()/TEXT("../assets/characters")/Skin/TEXT("animations")/(Clip+TEXT(".canim"));
   TestTrue(TEXT("Staged animation bytes exactly match the original art source"),FFileHelper::LoadFileToArray(Original,*Source)&&FFileHelper::LoadFileToArray(StagedBytes,*File)&&Original==StagedBytes);
#endif
   Report+=FString::Printf(TEXT("%s,%s,%d,%d,%.3f,%s\n"),*Skin,*Clip,int(Animation.tracks.size()),Matching,float(Animation.durationSeconds),*File); ++Loaded;
  }
 }
 TestTrue(TEXT("Unknown skin and path traversal never resolve a file"),CultHumanAnimation::Path(TEXT("../cultist_hooded"),TEXT("idle")).IsEmpty()&&CultHumanAnimation::Path(TEXT("cultist_magus"),TEXT("../idle")).IsEmpty());
 const auto Dir=FPaths::ProjectSavedDir()/TEXT("Reports"); IFileManager::Get().MakeDirectory(*Dir,true); TestTrue(TEXT("Actual staged clip/rig coverage is recorded"),FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("HumanAnimationCoverage.csv"))));
 AddInfo(FString::Printf(TEXT("Verified %d staged original clips and all three imported human rigs. Editor=%d Content=%s"),Loaded,int(WITH_EDITOR),*FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir())));
 Staged->Set(Before,ECVF_SetByCode); return true;
}

class FCultStagedHumanPoseObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; bool Entered=false; int Action=-1; double Began=FPlatformTime::Seconds(); float At=0;
 TArray<TWeakObjectPtr<ACultPopulation>> People; TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<AStaticMeshActor> Floor;
 int OldStaged=0;
 TArray<float> SampleTimes;
 const TArray<FString> Actions={TEXT("Pray"),TEXT("Build"),TEXT("Convert"),TEXT("SacrificePerformer"),TEXT("SacrificeVictim"),TEXT("Attack")};
 const TArray<FString> Clips={TEXT("channel"),TEXT("work"),TEXT("cast"),TEXT("sacrifice_performer"),TEXT("sacrifice_victim"),TEXT("attack")};
public:
 explicit FCultStagedHumanPoseObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Staged native human pose verification timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Entered) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("mi_go"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({5,6,11}); G->StartPlay(); Entered=true; } return false; }
  auto* PC=W->GetFirstPlayerController(); if(!PC||!PC->GetPawn()||W->GetTimeSeconds()<3) return false;
  if(Action<0) {
   if(auto* Hero=Cast<ACultUlhuPlayerPawn>(PC->GetPawn())) {
    FHitResult Support; FCollisionQueryParams Query(SCENE_QUERY_STAT(StagedPlayerSupport),false,Hero);
    Test->TestTrue(TEXT("Standalone Free Roam has actual supporting terrain collision"),W->LineTraceSingleByObjectType(Support,Hero->FeetLocation()+FVector(0,0,300),Hero->FeetLocation()-FVector(0,0,3000),FCollisionObjectQueryParams(ECC_WorldStatic),Query));
    Test->TestTrue(TEXT("Player has settled on the native floor before the staged-art fixture"),Hero->GetCharacterMovement()->IsMovingOnGround());
   }
   auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); auto* Mode=W->GetAuthGameMode<ACultUlhuPlayGameMode>(); const auto BeforeMode=G->SelectedMode;
   for(auto Choice:{ECultPlayMode::FreeRoam,ECultPlayMode::Domination,ECultPlayMode::Dotgoo}) {
    const FName Wanted(*UCultUlhuGameInstance::ModeStartLabel(Choice)); APlayerStart* Expected=nullptr;
    for(TActorIterator<APlayerStart> It(W);It;++It) if(It->PlayerStartTag==Wanted) Expected=*It;
    Test->TestNotNull(TEXT("Cooked mode starts retain runtime PlayerStartTag identity"),Expected);
    G->SelectedMode=Choice; Test->TestTrue(TEXT("Actual game-mode selection resolves the tagged start"),Expected&&Mode&&Mode->ChoosePlayerStart(PC)==Expected);
   }
   G->SelectedMode=BeforeMode;
   auto* Staged=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Animation.UseStagedClips")); OldStaged=Staged->GetInt(); Staged->Set(1,ECVF_SetByCode);
   Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,45000),FRotator::ZeroRotator); auto* Mesh=Floor->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Floor->SetActorScale3D(FVector(30,20,.2));
   Camera=W->SpawnActor<ACameraActor>(FVector(0,-1200,45500),FRotator::ZeroRotator); Camera->SetActorRotation((FVector(0,0,45100)-Camera->GetActorLocation()).Rotation()); PC->SetViewTarget(Camera.Get());
   for(int I=0;I<3;++I) { auto* P=W->SpawnActor<ACultPopulation>(FVector((I-1)*500,0,45100),FRotator(0,-90,0)); if(I>0) P->RecruitTo(0); if(I==2) P->SetSorcerer(true); P->GetCharacterMovement()->DisableMovement(); P->SetActorTickEnabled(false); People.Add(P); }
   Action=0; At=W->GetTimeSeconds();
  }
  SampleTimes.Add(float(W->GetTimeSeconds())); if(SampleTimes.Num()>12) SampleTimes.RemoveAt(0);
  for(auto& Weak:People) if(auto* P=Weak.Get()) { P->Order(Actions[Action],P->GetActorLocation(),nullptr,1000,true); P->Tick(W->GetDeltaSeconds()); }
  if(W->GetTimeSeconds()-At<.5f) return false;
  for(int I=0;I<People.Num();++I) {
   auto* P=People[I].Get(); Test->TestTrue(TEXT("Native actor retains every authored body part from staged art"),P&&P->HasAuthoredHumanModel()&&P->HumanPartCount()==6); if(!P) continue;
   UPoseableMeshComponent* Leader=nullptr; TArray<UPoseableMeshComponent*> Parts; P->GetComponents(Parts); for(auto* Part:Parts) if(!Part->LeaderPoseComponent.IsValid()) Leader=Part;
   bool Posed=false; if(Leader) { auto* Rig=Cast<USkeletalMesh>(Leader->GetSkinnedAsset()); const auto& Rest=Rig->GetRefSkeleton().GetRefBonePose(); for(int B=0;B<FMath::Min(Rest.Num(),Leader->BoneSpaceTransforms.Num());++B) Posed|=!Rest[B].Equals(Leader->BoneSpaceTransforms[B],.001f); }
   Test->TestTrue(FString::Printf(TEXT("%s/%s applies original keys to the real native body"),*HumanSkins()[I],*Actions[Action]),Posed);
   Test->TestTrue(TEXT("The selected activity matches its original clip, not just any animated pose"),PoseMatchesOriginal(Leader,CultHumanAnimation::Find(HumanSkins()[I],Clips[Action]),SampleTimes,P->GetUniqueID()));
  }
  if(++Action<Actions.Num()) { At=W->GetTimeSeconds(); return false; }
  for(auto& P:People) if(P.IsValid()) P->Destroy(); PC->SetViewTarget(PC->GetPawn()); if(Camera.IsValid()) Camera->Destroy(); if(Floor.IsValid()) Floor->Destroy();
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Animation.UseStagedClips"))->Set(OldStaged,ECVF_SetByCode);
  Test->AddInfo(FString::Printf(TEXT("Verified staged native civilian, cultist and sorcerer poses for six activities. Editor=%d"),int(WITH_EDITOR))); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultStagedHumanRuntimeTest,"CultUlhu.Runtime.StagedHumanPoses",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCultStagedHumanRuntimeTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStagedHumanPoseObservation(this)); return true; }
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultStagedHumanEditorTest,"CultUlhu.Menu.StagedHumanPoses",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultStagedHumanEditorTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultStagedHumanPoseObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
#endif
