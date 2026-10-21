#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/DebuggerCommands.h"
#include "Framework/Commands/InputBindingManager.h"
#endif

class FCultUlhuModule:public FDefaultGameModuleImpl {
#if WITH_EDITOR
 FDelegateHandle BeginHandle,SwitchHandle,EndHandle;
 static FName FilterOwner() { return TEXT("CultUlhuGameplayInput"); }
 void GameplaySession(bool Simulating) {
  auto& Bindings=FInputBindingManager::Get(); Bindings.UnregisterCommandFilterOwner(FilterOwner());
  if(!Simulating&&FPlayWorldCommands::IsRegistered()) {
   const auto Command=FPlayWorldCommands::Get().Simulate;
   if(Command.IsValid()) Bindings.AddCommandFilter(FilterOwner(),Command->GetBindingContext(),Command->GetCommandName(),FInputBindingManager::ECommandFilterType::DenyList);
  }
 }
 void EndSession(bool) { FInputBindingManager::Get().UnregisterCommandFilterOwner(FilterOwner()); }
#endif
public:
 void StartupModule() override {
  FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
  // ALT belongs to cult commands during PIE. Unreal's ALT+S shortcut otherwise
  // bubbles out of GameAndUI input and switches the player into editor simulation.
  // Filter command lookup only: no user key bindings or saved preferences change.
  BeginHandle=FEditorDelegates::BeginPIE.AddRaw(this,&FCultUlhuModule::GameplaySession);
  SwitchHandle=FEditorDelegates::OnSwitchBeginPIEAndSIE.AddRaw(this,&FCultUlhuModule::GameplaySession);
  EndHandle=FEditorDelegates::EndPIE.AddRaw(this,&FCultUlhuModule::EndSession);
#endif
 }
 void ShutdownModule() override {
#if WITH_EDITOR
  FEditorDelegates::BeginPIE.Remove(BeginHandle); FEditorDelegates::OnSwitchBeginPIEAndSIE.Remove(SwitchHandle); FEditorDelegates::EndPIE.Remove(EndHandle);
  EndSession(false);
#endif
  FDefaultGameModuleImpl::ShutdownModule();
 }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FCultUlhuModule,CultUlhu,"CultUlhu");
