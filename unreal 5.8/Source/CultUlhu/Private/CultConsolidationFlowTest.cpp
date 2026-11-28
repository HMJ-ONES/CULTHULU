#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultCharacterModel.h"
#include "CultSkillAttack.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/CultHomeScreenWidget.h"
#include "UI/CultRoomMenuWidget.h"
#include "UI/CultCharacterSelectWidget.h"
#include "UI/CultCharacterCardWidget.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/GarbageCollection.h"

namespace {
bool ClickText(UUserWidget* Widget,const FString& Label) {
 if(!Widget||!Widget->WidgetTree) return false;
 TArray<UWidget*> Children; Widget->WidgetTree->GetAllWidgets(Children);
 for(auto* Child:Children) if(auto* Text=Cast<UTextBlock>(Child);Text&&Text->GetText().ToString()==Label) {
  for(auto* Parent=Text->GetParent();Parent;Parent=Parent->GetParent()) if(auto* Button=Cast<UButton>(Parent)) { Button->OnClicked.Broadcast(); return true; }
 }
 return false;
}
}
class FCultConsolidationFlow:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0, Round=0; double Began=FPlatformTime::Seconds(); float At=0;
 bool SawArms=false,SawTrail=false,CheckedGrips=false;
public:
 explicit FCultConsolidationFlow(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Actual menu callback flow timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); if(!GI) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   TArray<UUserWidget*> UI;
   if(Phase==0||Phase==5) {
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultHomeScreenWidget::StaticClass(),true); if(UI.Num()!=1) return false;
    if(Phase==5) {
     Test->TestFalse(TEXT("Returning home clears match pause"),UGameplayStatics::IsGamePaused(W));
     if(++Round==3) return true;
     CollectGarbage(RF_NoFlags,true);
    }
    Test->TestTrue(TEXT("Real Home Domination button opens selection"),ClickText(UI[0],TEXT("DOMINATION"))); Phase=1; return false;
   }
   if(Phase==1) {
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultRoomMenuWidget::StaticClass(),true); if(UI.Num()!=1) return false;
    Test->TestTrue(TEXT("Room screen opens offline practice"),ClickText(UI[0],TEXT("PRACTICE / OFFLINE"))); UI.Empty(); Phase=6; return false;
   }
   if(Phase==6) {
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultCharacterCardWidget::StaticClass(),false);
    Test->TestEqual(TEXT("Menu keeps exactly sixteen released character cards"),UI.Num(),16);
    const TCHAR* Labels[]={TEXT("CTHULHU"),TEXT("YIG"),TEXT("DAGON")};
    const TCHAR* Ids[]={TEXT("cthulhu_avatar"),TEXT("yig"),TEXT("dagon")};
    bool Clicked=false; for(auto* Card:UI) if(ClickText(Card,Labels[Round])) { Clicked=true; break; }
    Test->TestTrue(TEXT("Real entity button selects the next fighter"),Clicked);
    Test->TestEqual(TEXT("Chosen ID comes from the actual card callback"),GI->SelectedCharacterId,FString(Ids[Round]));
    UI.Empty(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultCharacterSelectWidget::StaticClass(),true);
    Test->TestTrue(TEXT("Actual Enter Battle button starts the chosen mode"),UI.Num()==1&&ClickText(UI[0],TEXT("ENTER BATTLE"))); Phase=2; return false;
   }
   return false;
  }
  auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr; auto* Match=ACultMatchDirector::Find(W);
  if(!P||!Match) return false;
  if(Phase==2) {
   Test->TestEqual(TEXT("Menu callbacks reach a real Domination match"),Match->Mode(),ECultPlayMode::Domination);
   Test->TestEqual(TEXT("The new match installs the selected fighter"),P->GetCharacterId(),GI->SelectedCharacterId);
   if(Round==1) { Phase=7; return false; }
   Phase=9;
  }
  if(Phase==7) {
   if(Match->Warmup()>0) return false;
   P->ToggleCamera(); P->Smash();
   At=W->GetTimeSeconds(); Phase=8;
   return false;
  }
  if(Phase==8) {
   if(!CheckedGrips&&W->GetTimeSeconds()-At>.25f) {
    CheckedGrips=true;
    TArray<AActor*> Attached;
    P->GetAttachedActors(Attached);
    for(auto* Actor:Attached) if(auto* Model=Cast<ACultCharacterModel>(Actor)) {
     USkeletalMeshComponent* Torso=nullptr;
     USkeletalMeshComponent* Weapons=nullptr;
     for(const auto& Part:Model->Parts()) if(auto* Skin=Cast<USkeletalMeshComponent>(Part.Get())) {
      if(Skin->GetBoneIndex(TEXT("lefthand"))!=INDEX_NONE) Torso=Skin;
      if(Skin->GetBoneIndex(TEXT("weaponsroot"))!=INDEX_NONE) Weapons=Skin;
     }
     if(!Test->TestTrue(TEXT("Yig retains both the torso and original sword geometry"),Torso&&Weapons)) break;
     const FName Hands[]={TEXT("lefthand"),TEXT("righthand"),TEXT("lefthand_001"),TEXT("righthand_001")};
     const FName Swords[]={TEXT("sword_scimitar_lp__005"),TEXT("sword_scimitar_lp__006"),TEXT("sword_scimitar_lp__008"),TEXT("sword_scimitar_lp__009")};
     const FVector Grips[]={{221.34,-168.53,387.54},{63.12,-404.28,437.32},{313.81,-281.57,311.60},{75.15,-471.12,316.00}};
     const auto& Ref=Weapons->GetSkeletalMeshAsset()->GetRefSkeleton();
     for(int I=0;I<4;++I) {
      const int Bone=Weapons->GetBoneIndex(Swords[I]);
      if(!Test->TestTrue(TEXT("Each original blade has a dedicated binding bone"),Bone!=INDEX_NONE)) continue;
      FTransform Bind=Ref.GetRefBonePose()[Bone];
      for(int Parent=Ref.GetParentIndex(Bone);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent))
       Bind=Bind*Ref.GetRefBonePose()[Parent];
      const FVector Grip=Weapons->GetBoneTransform(Bone).TransformPosition(Bind.InverseTransformPosition(Grips[I]));
      const float Error=FVector::Dist(Grip,Torso->GetBoneLocation(Hands[I]));
      Test->TestTrue(FString::Printf(TEXT("Blade %d grip follows its own wrist (%.1f cm)"),I,Error),Error<45.f);
     }
    }
   }
   for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P) {
    TInlineComponentArray<UProceduralMeshComponent*> Meshes(*It);
    for(auto* Mesh:Meshes) if(Mesh->ComponentHasTag(TEXT("CultLocalMeleeView"))) {
     SawArms|=Mesh->IsVisible()&&Mesh->GetProcMeshSection(1)!=nullptr;
     if(Mesh->IsVisible()&&Mesh->GetProcMeshSection(0)&&W->GetTimeSeconds()-At>.30f&&!SawTrail) {
      SawTrail=true;
      FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/YigFirstPersonSwing.png"),false,false);
     }
    }
   }
   if(W->GetTimeSeconds()-At<.8f) return false;
   Test->TestTrue(TEXT("The actual Yig primary draws first-person arm geometry"),SawArms);
   Test->TestTrue(TEXT("The actual Yig primary draws its foreground slash"),SawTrail);
   P->ToggleCamera(); Phase=9;
  }
  if(Phase==9) {
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Pressed,1.f,false,FPlatformTime::Cycles64()));
   At=W->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3) {
   // Competitive menus must never pause the world.
   if(!Match->IsPaused()||W->GetTimeSeconds()-At<1) return false;
   Test->TestFalse(TEXT("Escape opens a local menu without pausing"),UGameplayStatics::IsGamePaused(W));
   Test->TestTrue(TEXT("World keeps advancing behind Escape menu"),W->GetTimeSeconds()-At>=1);
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Released,0.f,false,FPlatformTime::Cycles64()));
   GI->ReturnToMenu(); Phase=5; return false;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultConsolidationFlowTest,"CultUlhu.Menu.ConsolidationFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultConsolidationFlowTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultConsolidationFlow(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
