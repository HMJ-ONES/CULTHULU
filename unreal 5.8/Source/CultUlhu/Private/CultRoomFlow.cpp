#include "UI/CultHostedRoomWidget.h"
#include "CultMatchDirector.h"
#include "CultUlhuGameInstance.h"
#include "CultRoomService.h"
#include "UI/CultRoomMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
void UCultUlhuGameInstance::OpenModeEntry(ECultPlayMode Mode) {
 SelectedMode=Mode; RoomAction=ECultRoomAction::None; if(GetWorld()&&GetWorld()->GetName().Contains(TEXT("L_MainMenu"))) { RoomHosting=false; RoomPrivate=false; RoomCode.Empty(); RoomKey.Empty(); } if(Rooms_) Rooms_->StopSearch();
 if(auto* Widget=CreateWidget<UCultRoomMenuWidget>(this,UCultRoomMenuWidget::StaticClass())) ShowWidget(Widget,true);
}
void UCultUlhuGameInstance::BeginRoomAction(ECultRoomAction Action,const FString& Code) {
 if(Rooms_) Rooms_->StopSearch(); RoomAction=Action;
 if(Action==ECultRoomAction::Find) { if(Rooms_) Rooms_->Find(); return; }
 if(Action==ECultRoomAction::Join) {
  FCultRoomAddress R; FString Error; if(!CultRoomCode::Decode(Code,R,Error)) { RoomStatus=Error; RoomAction=ECultRoomAction::None; return; }
  if(R.Mode!=SelectedMode) { RoomStatus=TEXT("This code is for ")+ModeDisplayName(R.Mode)+TEXT(". Select that mode first."); RoomAction=ECultRoomAction::None; return; }
  RoomHost=R.Host; RoomPort=R.Port; RoomKey=R.Key; RoomPrivate=R.Private; RoomHosting=false; RoomCode=CultRoomCode::Encode(R); RoomStatus=TEXT("Choose your entity to join the room."); OpenCharacterSelect(SelectedMode); return;
 }
 RoomHosting=Action==ECultRoomAction::Host||Action==ECultRoomAction::Private; RoomPrivate=Action==ECultRoomAction::Private;
 RoomKey=RoomPrivate?FString::Printf(TEXT("%08X%08X"),FGuid::NewGuid().A,FGuid::NewGuid().C):FString(); RoomHost=CultRoomCode::LocalAddress(); RoomPort=7777; FParse::Value(FCommandLine::Get(),TEXT("port="),RoomPort); RoomPort=FMath::Clamp(RoomPort,1024,65535);
 RoomCode.Empty(); RoomStatus.Empty(); OpenCharacterSelect(SelectedMode);
}
void UCultUlhuGameInstance::PublicRoomFound(const FString& Host,int Port) {
 FCultRoomAddress R{Host,FString(),uint16(Port),SelectedMode,false}; BeginRoomAction(ECultRoomAction::Join,CultRoomCode::Encode(R));
}
bool UCultUlhuGameInstance::CompleteRoomJoin() {
 if(RoomAction!=ECultRoomAction::Join) return false;
 if(auto* PC=GetWorld()->GetFirstPlayerController()) {
  bPlayChosen=true; FreeRoamReady=false; FreeRoamMenuOpen=false; ClearMenuWidgets(); UGameplayStatics::SetGamePaused(this,false);
  PC->ClientTravel(FString::Printf(TEXT("%s:%d?CultProtocol=1?CultMode=%d?CultKey=%s"),*RoomHost,RoomPort,int(SelectedMode),*RoomKey),TRAVEL_Absolute); RoomAction=ECultRoomAction::None; return true;
 }
 RoomStatus=TEXT("No local player is ready to connect. Try again."); return true;
}
void UCultUlhuGameInstance::CopyRoomCode() { if(!RoomCode.IsEmpty()) { FPlatformApplicationMisc::ClipboardCopy(*RoomCode); RoomStatus=TEXT("Room code copied. Send it to your friend."); } }
void UCultUlhuGameInstance::ValidateRoomLogin(const FString& Options,FString& Error) const {
 if(!RoomHosting) return;
 if(UGameplayStatics::ParseOption(Options,TEXT("CultProtocol"))!=TEXT("1")||UGameplayStatics::ParseOption(Options,TEXT("CultMode"))!=FString::FromInt(int(SelectedMode))) { Error=TEXT("Use this room's code with the same game version and mode."); return; }
 if(RoomPrivate&&UGameplayStatics::ParseOption(Options,TEXT("CultKey"))!=RoomKey) Error=TEXT("Private room code is incorrect or expired.");
}

void UCultUlhuGameInstance::LeaveCompetitiveMatch() {
 if(SelectedMode==ECultPlayMode::FreeRoam) return;
 if(GetWorld()->GetNetMode()==NM_ListenServer) { if(auto* PC=GetWorld()->GetFirstPlayerController()) if(auto* M=ACultMatchDirector::Find(GetWorld())) M->RemoveHumanPlayer(PC); HostedRoomLobby=true; ShowHostedRoomLobby(); return; }
 ReturnToMenu();
}
void UCultUlhuGameInstance::ShowHostedRoomLobby() { HostedRoomLobby=true; if(auto* Widget=CreateWidget<UCultHostedRoomWidget>(this,UCultHostedRoomWidget::StaticClass())) ShowWidget(Widget,true); }
void UCultUlhuGameInstance::RejoinHostedRoom() {
 auto* M=ACultMatchDirector::Find(GetWorld()); auto* PC=GetWorld()->GetFirstPlayerController(); if(!M||!PC) return;
 if(!M->AddHumanPlayer(PC,SelectedCharacterId)) { RoomStatus=TEXT("All player slots are occupied. Wait for a free slot."); return; }
 HostedRoomLobby=false; ClearMenuWidgets(); PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false;
 if(M->IsPaused()) M->TogglePause();
}
