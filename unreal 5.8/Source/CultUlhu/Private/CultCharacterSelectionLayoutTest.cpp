#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "UI/CultCharacterSelectWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "UObject/StrongObjectPtr.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UI/CultCharacterCardWidget.h"
#include "CultCanonicalKit.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

class FCultCharacterSelectionCapture:public IAutomationLatentCommand {
 FAutomationTestBase* Test; bool Ready=false; double Began=FPlatformTime::Seconds();
public:
 explicit FCultCharacterSelectionCapture(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("Character selection capture timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown||!W->GetName().Contains(TEXT("L_MainMenu"))) return false;
  if(!Ready) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); Ready=true; return false; }
  // A cold editor may still be compiling the imported source materials.
  // Capturing its temporary white fallback is not a review of the actual art.
  if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()) return false;
  TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Menus,UCultCharacterSelectWidget::StaticClass(),true); if(Menus.Num()!=1) return false;
  auto* Menu=Cast<UCultCharacterSelectWidget>(Menus[0]);
  const auto& Roster=UCultUlhuGameInstance::Roster();
  for(int Index=0;Index<Roster.Num();++Index) if(Roster[Index].Id==TEXT("cthulhu_avatar")) Menu->SelectCharacter(Index);
  Menu->RemoveFromParent();
  FString Label; FParse::Value(FCommandLine::Get(),TEXT("CharacterMenuCapture="),Label); if(Label!=TEXT("Before")&&Label!=TEXT("After")) Label=TEXT("Review");
  FWidgetRenderer Renderer(true,true);
  for(const FIntPoint Size:{FIntPoint(960,540),FIntPoint(1280,720),FIntPoint(1920,1080),FIntPoint(1280,540)}) {
   const float DPI=GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
   TSharedRef<SWidget> Root=SNew(SDPIScaler).DPIScale(DPI)[Menu->TakeWidget()];
   TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size),TF_Bilinear,true));
   Test->TestNotNull(TEXT("Native menu render target exists"),Target.Get()); if(!Target.IsValid()) return true;
   for(int Settle=0;Settle<4;++Settle) { Menu->ForceLayoutPrepass(); Renderer.DrawWidget(Target.Get(),Root,FVector2D(Size),0.f); FlushRenderingCommands(); }
   TArray<FColor> Pixels; Test->TestTrue(TEXT("Native Slate menu readback succeeds"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
   Test->TestEqual(TEXT("Menu output has exact requested dimensions"),Pixels.Num(),Size.X*Size.Y);
   TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,MakeArrayView(Pixels),PNG);
   const auto Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/CharacterSelection_%s_%dx%d.png"),*Label,Size.X,Size.Y));
   Test->TestTrue(TEXT("Native menu comparison image saved"),FFileHelper::SaveArrayToFile(PNG,*Path));
   Test->AddInfo(FString::Printf(TEXT("Character selection %s %dx%d at real engine DPI %.3f"),*Label,Size.X,Size.Y,DPI));
  }
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultCharacterSelectionCaptureTest,"CultUlhu.Menu.CharacterSelectionCapture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultCharacterSelectionCaptureTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultCharacterSelectionCapture(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }

struct FCultCharacterSelectionTestAccess {
 static FBox2D Bounds(UWidget* Widget) { const auto& G=Widget->GetCachedGeometry(); const auto P=G.GetAbsolutePosition(); return FBox2D(P,P+G.GetLocalSize()*G.GetAccumulatedLayoutTransform().GetScale()); }
 static bool Inside(const FBox2D& A,const FBox2D& B) { return A.Min.X>=B.Min.X-1&&A.Min.Y>=B.Min.Y-1&&A.Max.X<=B.Max.X+1&&A.Max.Y<=B.Max.Y+1; }
 static void Click(UCultCharacterSelectWidget* Menu,int Index) { Menu->Cards_[Index]->CardButton_->OnClicked.Broadcast(); }
 static void Verify(FAutomationTestBase* T,UCultCharacterSelectWidget* Menu,UCultUlhuGameInstance* GI,ECultPlayMode Mode,int Index,FIntPoint Size,float& MinFont,float& MinGlyph) {
  const auto& Roster=UCultUlhuGameInstance::Roster(); const FBox2D Frame(FVector2D::ZeroVector,FVector2D(Size));
  T->TestEqual(TEXT("Released roster has exactly sixteen cards"),Menu->Cards_.Num(),16);
  T->TestEqual(TEXT("Real card callback updates game selection"),GI->SelectedCharacterId,Roster[Index].Id);
  const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
  const auto GridViewport=Bounds(Menu->Grid_->GetParent());
  for(auto* Card:Menu->Cards_) {
   T->TestTrue(TEXT("Every entity button is enabled and wholly visible in the roster viewport"),Card->CardButton_->GetIsEnabled()&&Inside(Bounds(Card->CardButton_),GridViewport)&&Inside(Bounds(Card->CardButton_),Frame));
   auto* Name=Card->NameText_; const auto& G=Name->GetCachedGeometry(); const float Scale=G.GetAccumulatedLayoutTransform().GetScale();
   const auto Glyph=Measure->Measure(Name->GetText().ToString(),Name->GetFont(),Scale); const auto Allocated=G.GetLocalSize()*Scale;
   T->TestTrue(FString::Printf(TEXT("Complete entity name fits its measured glyph bounds: %s"),*Name->GetText().ToString()),Glyph.X<=Allocated.X+1&&Glyph.Y<=Allocated.Y+1&&Inside(Bounds(Name),Bounds(Card->CardButton_)));
   MinFont=FMath::Min(MinFont,float(Name->GetFont().Size)*Scale); MinGlyph=FMath::Min(MinGlyph,float(Glyph.Y));
  }
  T->TestTrue(TEXT("Back and primary actions remain on screen"),Inside(Bounds(Menu->BackButton_),Frame)&&Inside(Bounds(Menu->PlayButton_),Frame));
  T->TestTrue(TEXT("Mode guidance remains visible above the action row"),Inside(Bounds(Menu->ModeHint_),Frame)&&Bounds(Menu->ModeHint_).Max.Y<=Bounds(Menu->PlayButton_).Min.Y+1);
  T->TestTrue(TEXT("All roster cards clear the primary action row"),Bounds(Menu->Grid_).Min.Y+Menu->Grid_->GetDesiredSize().Y*Menu->Grid_->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale()<=Bounds(Menu->PlayButton_).Min.Y+1);
  const auto DetailViewport=Bounds(Menu->DetailText_->GetParent()->GetParent());
  T->TestEqual(TEXT("Four distinct ability rows exist"),Menu->AbilityNames_.Num(),4);
  for(int Slot=0;Slot<4;++Slot) {
   T->TestTrue(TEXT("Ability name remains visible in its detail viewport"),Inside(Bounds(Menu->AbilityNames_[Slot]),DetailViewport));
   T->TestTrue(FString::Printf(TEXT("Ability metadata remains visible: %s / %s / slot %d / %dx%d"),*Roster[Index].Id,*UCultUlhuGameInstance::ModeDisplayName(Mode),Slot,Size.X,Size.Y),Inside(Bounds(Menu->AbilityMeta_[Slot]),DetailViewport));
  }
  cultulhu::CharacterDef Definition; T->TestTrue(TEXT("Selected original kit can be read"),CultCanonicalKit::Load(Roster[Index].Id,Definition));
  const cultulhu::SpellDef* Spells[]={&Definition.qAbility,&Definition.fAbility,&Definition.rAbility};
  for(int Slot=0;Slot<3;++Slot) {
   T->TestEqual(TEXT("Selected ability name matches original source"),Menu->AbilityNames_[Slot]->GetText().ToString(),FString(UTF8_TO_TCHAR(Spells[Slot]->name.c_str())));
   const auto Meta=Menu->AbilityMeta_[Slot]->GetText().ToString();
   T->TestTrue(TEXT("Ability cooldown and stamina match source values"),Meta.Contains(FString::Printf(TEXT(" / %gs /"),double(Spells[Slot]->cooldownSec)))&&Meta.Contains(FString::Printf(TEXT("/ %g STAMINA"),double(Spells[Slot]->staminaCost))));
   if(Mode==ECultPlayMode::Domination&&Spells[Slot]->effectKind=="summon") T->TestTrue(TEXT("Domination explains its existing invocation strike"),Meta.Contains(TEXT("AREA STRIKE")));
   if(Mode!=ECultPlayMode::FreeRoam&&Spells[Slot]->effectKind=="heal") T->TestTrue(TEXT("Competitive healing explains its hero-only target rule"),Meta.Contains(TEXT("ALLIED HEROES")));
  }
  T->TestTrue(TEXT("Health display respects source versus competitive normalization"),Menu->DetailStats_->GetText().ToString().Contains(FString::Printf(TEXT(" / HP %.0f /"),Mode==ECultPlayMode::FreeRoam?Definition.maxHp:100.f)));
  if(Mode==ECultPlayMode::Dotgoo&&Roster[Index].Id==TEXT("cthulhu_avatar")) {
   T->TestEqual(TEXT("MOBA Cthulhu explains the actual alternate attack"),Menu->AbilityNames_[3]->GetText().ToString(),FString(TEXT("Minion Sling")));
   T->TestTrue(TEXT("Minion sling shows its actual cooldown"),Menu->AbilityMeta_[3]->GetText().ToString().Contains(TEXT("20s")));
  } else T->TestEqual(TEXT("Alternate attack name matches original source"),Menu->AbilityNames_[3]->GetText().ToString(),FString(UTF8_TO_TCHAR(Definition.rightClick.name.c_str())));
  T->TestTrue(TEXT("Belief guidance appears only in Free Roam"),Menu->ModeHint_->GetText().ToString().Contains(TEXT("beliefs"))==(Mode==ECultPlayMode::FreeRoam));
 }
};
class FCultCharacterSelectionLayout:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Case=0; double Began=FPlatformTime::Seconds(); TArray<FString> Results;
public:
 explicit FCultCharacterSelectionLayout(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Character selection layout verification timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown||!W->GetName().Contains(TEXT("L_MainMenu"))) return false;
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance());
  const FIntPoint Sizes[]={FIntPoint(960,540),FIntPoint(1280,720),FIntPoint(1920,1080),FIntPoint(1280,540)};
  const ECultPlayMode Modes[]={ECultPlayMode::FreeRoam,ECultPlayMode::Domination,ECultPlayMode::Dotgoo};
  const auto Size=Sizes[Case%4]; const auto Mode=Modes[Case/4]; GI->OpenCharacterSelect(Mode);
  TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Menus,UCultCharacterSelectWidget::StaticClass(),true);
  if(Menus.Num()!=1) { Test->AddError(TEXT("Expected one character selection widget")); return true; }
  auto* Menu=Cast<UCultCharacterSelectWidget>(Menus[0]); Menu->RemoveFromParent(); const float DPI=GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
  FWidgetRenderer Renderer(true,true); TSharedRef<SWidget> Root=SNew(SDPIScaler).DPIScale(DPI)[Menu->TakeWidget()];
  TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size),TF_Bilinear,true));
  float MinFont=1000,MinGlyph=1000;
  for(int Index=0;Index<UCultUlhuGameInstance::Roster().Num();++Index) {
   FCultCharacterSelectionTestAccess::Click(Menu,Index);
   for(int Settle=0;Settle<4;++Settle) { Menu->ForceLayoutPrepass(); Renderer.DrawWidget(Target.Get(),Root,FVector2D(Size),0.f); FlushRenderingCommands(); }
   FCultCharacterSelectionTestAccess::Verify(Test,Menu,GI,Mode,Index,Size,MinFont,MinGlyph);
  }
  const auto Row=FString::Printf(TEXT("{\"mode\":\"%s\",\"width\":%d,\"height\":%d,\"engine_dpi\":%.4f,\"card_callbacks\":16,\"minimum_font_units\":%.3f,\"minimum_glyph_height_pixels\":%.3f}"),*UCultUlhuGameInstance::ModeDisplayName(Mode),Size.X,Size.Y,DPI,MinFont,MinGlyph);
  Results.Add(Row); Test->AddInfo(TEXT("Character layout ")+Row);
  if(++Case<12) return false;
  FFileHelper::SaveStringToFile(TEXT("[\n")+FString::Join(Results,TEXT(",\n"))+TEXT("\n]\n"),*(FPaths::ProjectSavedDir()/TEXT("Reports/CharacterSelectionLayout.json")));
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultCharacterSelectionLayoutTest,"CultUlhu.Menu.CharacterSelectionLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultCharacterSelectionLayoutTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultCharacterSelectionLayout(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
