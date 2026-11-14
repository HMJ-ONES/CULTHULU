#include "CultLobbySubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace CultLobby
{
 const FString Base=TEXT("https://cultulhu-lobby.pages.dev/api/v1");
 const FString Site=TEXT("https://cultulhu-lobby.pages.dev");
 using J=TSharedPtr<FJsonObject>;
 J Object(J Parent,const TCHAR* Key) { const J* Result=nullptr; return Parent&&Parent->TryGetObjectField(Key,Result)?*Result:nullptr; }
 FString String(J Parent,const TCHAR* Key) { FString Result; if(Parent) Parent->TryGetStringField(Key,Result); return Result; }
 double Number(J Parent,const TCHAR* Key) { double Result=0; if(Parent) Parent->TryGetNumberField(Key,Result); return Result; }
 bool Identifier(const FString& S) {
  if(S.Len()<8||S.Len()>64) return false;
  for(TCHAR C:S) if(!((C>='a'&&C<='z')||(C>='A'&&C<='Z')||(C>='0'&&C<='9')||C=='_'||C=='-')) return false;
  return true;
 }
 // Cleanup owns its credentials, so late responses after Cancel/Deinitialize cannot leak a seat/lobby.
 void Cleanup(const FString& Path,J Body,const FString& Host=FString()) {
  if(Path.IsEmpty()) return;
  auto R=FHttpModule::Get().CreateRequest(); R->SetURL(Base+Path); R->SetVerb(Host.IsEmpty()?TEXT("POST"):TEXT("DELETE")); R->SetTimeout(10);
  R->SetHeader(TEXT("Content-Type"),TEXT("application/json"));
  if(!Host.IsEmpty()) R->SetHeader(TEXT("X-Host-Token"),Host);
  if(Body) { FString Text; FJsonSerializer::Serialize(Body.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)); R->SetContentAsString(Text); }
  R->ProcessRequest(); // best effort; server TTL covers process termination/offline shutdown
 }
 J ReservationRef(const FString& Id,const FString& Token) {
  auto B=MakeShared<FJsonObject>(); B->SetStringField(TEXT("reservationId"),Id); B->SetStringField(TEXT("reservationToken"),Token); return B;
 }
 void CleanupLate(J Reply) {
  auto R=Object(Reply,TEXT("reservation")); const FString Id=String(R,TEXT("lobbyId"));
  if(Identifier(Id)) Cleanup(TEXT("/lobbies/")+Id+TEXT("/leave"),ReservationRef(String(R,TEXT("id")),String(R,TEXT("token"))));
  const FString Host=String(Reply,TEXT("hostToken")),Lobby=String(Object(Reply,TEXT("lobby")),TEXT("id"));
  if(!Host.IsEmpty()&&Identifier(Lobby)) Cleanup(TEXT("/lobbies/")+Lobby,nullptr,Host);
 }
}
using namespace CultLobby;
UCultLobbySubsystem::UCultLobbySubsystem() = default;

FString UCultLobbySubsystem::WireMode(ECultPlayMode Mode) {
 switch(Mode) { case ECultPlayMode::FreeRoam:return TEXT("free_roam"); case ECultPlayMode::Domination:return TEXT("domination"); case ECultPlayMode::Dotgoo:return TEXT("dotgoo"); }
 return FString();
}
bool UCultLobbySubsystem::IsRadminAddress(const FString& Address) {
 TArray<FString> Parts; Address.ParseIntoArray(Parts,TEXT("."),false); if(Parts.Num()!=4||Parts[0]!=TEXT("26")) return false;
 for(const FString& P:Parts) {
  if(P.IsEmpty()||P.Len()>3||(P.Len()>1&&P[0]=='0')) return false;
  for(TCHAR C:P) if(C<'0'||C>'9') return false;
  if(FCString::Atoi(*P)>255) return false;
 }
 return Address!=TEXT("26.0.0.0")&&Address!=TEXT("26.255.255.255");
}
FString UCultLobbySubsystem::LocalRadminAddress() {
 auto* S=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM); if(!S) return FString();
 TArray<TSharedPtr<FInternetAddr>> Addresses; S->GetLocalAdapterAddresses(Addresses);
 for(const auto& A:Addresses) if(A&&IsRadminAddress(A->ToString(false))) return A->ToString(false);
 return FString();
}
bool UCultLobbySubsystem::ParseLobbyInput(const FString& Input,FString& Id,FString& PrivateInvite) {
 Id.Empty(); PrivateInvite.Empty(); FString S=Input.TrimStartAndEnd(); if(S.Len()>256) return false;
 const FString Web=Site+TEXT("/join/"),URI=TEXT("cultulhu://join/");
 if(S.StartsWith(Web,ESearchCase::IgnoreCase)) S=S.Mid(Web.Len());
 else if(S.StartsWith(URI,ESearchCase::IgnoreCase)) S=S.Mid(URI.Len());
 else if(S.Contains(TEXT(":"))||S.Contains(TEXT("/"))) return false;
 // A single trailing slash from a copied web address is harmless. Queries/commands are rejected.
 S.RemoveFromEnd(TEXT("/")); if(!Identifier(S)) return false;
 FGuid Guid;
 if(S.StartsWith(TEXT("lby_"))||FGuid::ParseExact(S,EGuidFormats::DigitsWithHyphens,Guid)) { Id=S; return true; }
 // Production invitations are short uppercase strings with optional hyphen separators.
 if(S.Len()<=20) { PrivateInvite=S.ToUpper(); return true; }
 return false;
}
void UCultLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection) {
 Super::Initialize(Collection);
 checkf(GetClass()->GetStructureSize()==sizeof(UCultLobbySubsystem),TEXT("CultLobbySubsystem reflected layout is stale; rebuild the canonical project."));
 Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UCultLobbySubsystem::Tick),.25f);
 if(GEngine) { GEngine->OnNetworkFailure().AddUObject(this,&UCultLobbySubsystem::NetworkFailure); GEngine->OnTravelFailure().AddUObject(this,&UCultLobbySubsystem::TravelFailure); }
 if(!IsRunningCommandlet()&&!FParse::Param(FCommandLine::Get(),TEXT("unattended"))&&!FParse::Param(FCommandLine::Get(),TEXT("CultOfflineNetworking"))) EnsureSession([]{});
}
void UCultLobbySubsystem::Deinitialize() {
 LeaveLobby(); ShuttingDown=true; SessionWaiters.Empty(); SessionToken.Empty(); NetworkPassword.Empty(); RequiredNetworkPassword.Empty();
 FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
 if(GEngine) { GEngine->OnNetworkFailure().RemoveAll(this); GEngine->OnTravelFailure().RemoveAll(this); }
 Super::Deinitialize();
}
void UCultLobbySubsystem::SetStatus(ECultLobbyState NewState,const FString& Message) {
 State=NewState; Status=Message;
 if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->RoomStatus=Message;
 OnStatusChanged.Broadcast(State,Status);
}
void UCultLobbySubsystem::Request(const FString& Verb,const FString& Path,FJson Body,FReply Reply,const FString& HostCredential,bool Authenticate) {
 auto R=FHttpModule::Get().CreateRequest(); R->SetURL(Base+Path); R->SetVerb(Verb); R->SetTimeout(15);
 R->SetHeader(TEXT("Content-Type"),TEXT("application/json")); R->SetHeader(TEXT("Accept"),TEXT("application/json"));
 if(!HostCredential.IsEmpty()) R->SetHeader(TEXT("X-Host-Token"),HostCredential);
 else if(Authenticate&&!SessionToken.IsEmpty()) R->SetHeader(TEXT("Authorization"),TEXT("Bearer ")+SessionToken);
 if(Body) { FString Text; FJsonSerializer::Serialize(Body.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)); R->SetContentAsString(Text); }
 const int32 Epoch=Generation; const bool SessionRequest=Path==TEXT("/session"); TWeakObjectPtr<UCultLobbySubsystem> Weak(this);
 R->OnProcessRequestComplete().BindLambda([Weak,Epoch,SessionRequest,Reply=MoveTemp(Reply)](FHttpRequestPtr,FHttpResponsePtr Response,bool OK) mutable {
  FJson Json; int32 Code=0;
  if(OK&&Response) {
   Code=Response->GetResponseCode();
   if(Response->GetContentLength()<=65536) FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Json);
   if(!Json) Json=MakeShared<FJsonObject>();
   Json->SetNumberField(TEXT("_retryAfter"),FMath::Clamp(FCString::Atoi(*Response->GetHeader(TEXT("Retry-After"))),10,300));
  }
  auto* Self=Weak.Get();
  if(!Self||Self->ShuttingDown||(!SessionRequest&&Self->Generation!=Epoch)) { CleanupLate(Json); return; }
  if(Code==401) { Self->SessionToken.Empty(); Self->SessionExpires=0; }
  Reply(Code,Json);
 });
 if(!R->ProcessRequest()) { auto Callback=R->OnProcessRequestComplete(); Callback.ExecuteIfBound(R,nullptr,false); R->OnProcessRequestComplete().Unbind(); }
}
void UCultLobbySubsystem::EnsureSession(TFunction<void()> Ready) {
 const int32 Epoch=Generation;
 if(!SessionToken.IsEmpty()&&FDateTime::UtcNow().ToUnixTimestamp()+60<SessionExpires) { Ready(); return; }
 SessionWaiters.Add([this,Epoch,Ready=MoveTemp(Ready)]() mutable { if(Generation==Epoch) Ready(); });
 if(SessionBusy) return; SessionBusy=true;
 Request(TEXT("POST"),TEXT("/session"),nullptr,[this](int32 Code,FJson Json) {
  SessionBusy=false; SessionToken=String(Json,TEXT("token")); PlayerId=String(Json,TEXT("playerId")); SessionExpires=Number(Json,TEXT("expiresAt"))/1000.;
  auto Waiters=MoveTemp(SessionWaiters); SessionWaiters.Empty();
  if(Code<200||Code>=300||SessionToken.IsEmpty()||PlayerId.IsEmpty()) { Fail(Code,Json); return; }
  for(auto& Ready:Waiters) Ready();
 },FString(),false);
}
void UCultLobbySubsystem::Fail(int32 Code,FJson Json) {
 FString Message=TEXT("Lobby service unavailable. Please try again.");
 const FString Error=String(Object(Json,TEXT("error")),TEXT("code"));
 if(Code==429) Message=TEXT("Lobby service is busy. Please wait a moment before retrying.");
 else if(Code==401) Message=TEXT("Your lobby session expired. Please try again.");
 else if(Error==TEXT("lobby_full")) Message=TEXT("This match is full. Search for another match.");
 else if(Error==TEXT("invite_required")||Error==TEXT("invite_invalid")) Message=TEXT("This private match needs its invitation code.");
 else if(Error==TEXT("not_found")||Error==TEXT("not_joinable")||Code==410) Message=TEXT("This match has closed or is no longer accepting players.");
 else if(Error==TEXT("bad_password")) Message=TEXT("This room needs its lobby password. Ask the host for an invitation.");
 ReleaseReservation(); SetStatus(ECultLobbyState::Error,Message);
}
bool UCultLobbySubsystem::ConfigureRadminNetwork(const FString& Name,const FString& Password) {
 if(Name.TrimStartAndEnd().Len()<3||Name.Len()>60||Password.Len()<6||Password.Len()>60) {
  SetStatus(ECultLobbyState::Error,TEXT("Hosting needs your Radmin network name and its password (6–60 characters).")); return false;
 }
 NetworkName=Name.TrimStartAndEnd(); NetworkPassword=Password; return true;
}
void UCultLobbySubsystem::BrowseLobbies(ECultPlayMode Mode) {
 if(!WireMode(Mode).IsEmpty()) FPlatformProcess::LaunchURL(*(Site+TEXT("/lobbies?mode=")+WireMode(Mode)+TEXT("&searching=1")),nullptr,nullptr);
}
void UCultLobbySubsystem::QuickPlay(ECultPlayMode Mode,bool OpenBrowser) {
 LeaveLobby(); SelectedMode=Mode; if(WireMode(Mode).IsEmpty()) return;
 SetStatus(ECultLobbyState::Searching,TEXT("Searching for a match...")); NextPoll=FPlatformTime::Seconds(); PollFailures=0;
 if(OpenBrowser) BrowseLobbies(Mode);
}
bool UCultLobbySubsystem::Compatible(FJson Lobby,bool RequireJoinable) const {
 bool Joinable=false; if(Lobby) Lobby->TryGetBoolField(TEXT("joinable"),Joinable);
 return Lobby&&Identifier(String(Lobby,TEXT("id")))&&String(Lobby,TEXT("mode"))==WireMode(SelectedMode)&&String(Lobby,TEXT("gameVersion"))==TEXT("0.1.0")&&String(Lobby,TEXT("transport"))==TEXT("radmin_desktop")&&(!RequireJoinable||Joinable);
}
void UCultLobbySubsystem::Poll() {
 NextPoll=0;
 EnsureSession([this] {
  if(State!=ECultLobbyState::Searching) return;
  auto Body=MakeShared<FJsonObject>(); Body->SetStringField(TEXT("mode"),WireMode(SelectedMode)); Body->SetStringField(TEXT("gameVersion"),TEXT("0.1.0"));
  Request(TEXT("POST"),TEXT("/matchmaking/quickplay"),Body,[this](int32 Code,FJson Json) {
   if(Code>=200&&Code<300&&String(Json,TEXT("result"))==TEXT("reserved")) {
    if(!Compatible(Object(Json,TEXT("lobby")),false)) { CleanupLate(Json); SetStatus(ECultLobbyState::Error,TEXT("The returned match uses a different mode or game version.")); return; }
    AcceptReservation(Json); return;
   }
   if(Code>=200&&Code<300&&String(Json,TEXT("result"))==TEXT("no_lobby")) {
    PollFailures=0; SetStatus(ECultLobbyState::Searching,TEXT("Searching for a match... No open room yet.")); NextPoll=FPlatformTime::Seconds()+10; return;
   }
   if((Code==0||Code==401||Code==429||Code>=500)&&++PollFailures<=3) {
    SetStatus(ECultLobbyState::Searching,TEXT("Lobby service is busy. Retrying..."));
    NextPoll=FPlatformTime::Seconds()+(Code==429?Number(Json,TEXT("_retryAfter")):10*(1<<PollFailures)); return;
   }
   Fail(Code,Json);
  });
 });
}
void UCultLobbySubsystem::JoinByCode(const FString& Input,ECultPlayMode Mode) {
 LeaveLobby(); SelectedMode=Mode; FString Id,Code;
 if(!ParseLobbyInput(Input,Id,Code)) { SetStatus(ECultLobbyState::Error,TEXT("Paste a CULT-ULHU join link, lobby ID, or private invitation code.")); return; }
 SetStatus(ECultLobbyState::Connecting,TEXT("Checking invitation..."));
 EnsureSession([this,Id,Code] {
  if(!Code.IsEmpty()) {
   auto B=MakeShared<FJsonObject>(); B->SetStringField(TEXT("inviteCode"),Code);
   Request(TEXT("POST"),TEXT("/invitations/resolve"),B,[this,Code](int32 StatusCode,FJson Json) {
    if(StatusCode<200||StatusCode>=300) { Fail(StatusCode,Json); return; }
    auto Lobby=Object(Json,TEXT("lobby")); if(!Compatible(Lobby)) { SetStatus(ECultLobbyState::Error,TEXT("This invitation is closed, incompatible, or for another mode. Select its mode first.")); return; }
    Reserve(String(Lobby,TEXT("id")),Code);
   },FString(),false);
  } else {
   Request(TEXT("GET"),TEXT("/lobbies/")+Id,nullptr,[this,Id](int32 StatusCode,FJson Json) {
    if(StatusCode<200||StatusCode>=300) { Fail(StatusCode,Json); return; }
    if(!Compatible(Object(Json,TEXT("lobby")))) { SetStatus(ECultLobbyState::Error,TEXT("This room is closed, incompatible, or for another mode.")); return; }
    Reserve(Id,FString());
   });
  }
 });
}
void UCultLobbySubsystem::Reserve(const FString& Id,const FString& Code) {
 JoiningInvite=Code;
 auto B=MakeShared<FJsonObject>(); if(!Code.IsEmpty()) B->SetStringField(TEXT("inviteCode"),Code);
 Request(TEXT("POST"),TEXT("/lobbies/")+Id+TEXT("/reserve"),B,[this](int32 StatusCode,FJson Json) {
  if(StatusCode<200||StatusCode>=300) { Fail(StatusCode,Json); return; } AcceptReservation(Json);
 });
}
void UCultLobbySubsystem::AcceptReservation(FJson Json) {
 auto R=Object(Json,TEXT("reservation")),C=Object(R,TEXT("connection"));
 LobbyId=String(R,TEXT("lobbyId")); ReservationId=String(R,TEXT("id")); ReservationToken=String(R,TEXT("token")); ReservationExpires=Number(R,TEXT("expiresAt"))/1000.;
 HostAddress=String(C,TEXT("hostAddress")); const double Port=Number(C,TEXT("port")); HostPort=7777;
 RequiredNetwork=String(C,TEXT("networkName")); RequiredNetworkPassword=String(C,TEXT("networkPassword"));
 // Reconstruct a validated address, never execute connectCommand/launchUri received over HTTP.
 if(!Identifier(LobbyId)||ReservationId.IsEmpty()||ReservationToken.IsEmpty()||!IsRadminAddress(HostAddress)||Port!=7777||RequiredNetwork.Len()>60||RequiredNetworkPassword.Len()>60||ReservationExpires<=FDateTime::UtcNow().ToUnixTimestamp()) {
  ReleaseReservation(); SetStatus(ECultLobbyState::Error,TEXT("The host returned unsupported connection details.")); return;
 }
 SetStatus(ECultLobbyState::WaitingForRadmin,TEXT("Match reserved. Join the host's Radmin network, then press Connect."));
 // Adapter presence does not prove membership of the named network. Explicit acknowledgement is required.
 if(!LocalRadminAddress().IsEmpty()&&AcknowledgedNetworks.Contains(RequiredNetwork+TEXT("\n")+RequiredNetworkPassword)) ContinueRadminConnection();
}
void UCultLobbySubsystem::ContinueRadminConnection() {
 if(State!=ECultLobbyState::WaitingForRadmin||ReservationId.IsEmpty()) return;
 if(LocalRadminAddress().IsEmpty()) { SetStatus(ECultLobbyState::WaitingForRadmin,TEXT("Start Radmin VPN, join the network shown below, then press Connect.")); return; }
 if(FDateTime::UtcNow().ToUnixTimestamp()+10>=ReservationExpires) { ReleaseReservation(); SetStatus(ECultLobbyState::Error,TEXT("Your reserved place expired. Please join again.")); return; }
 auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr; if(!PC) return;
 AcknowledgedNetworks.Add(RequiredNetwork+TEXT("\n")+RequiredNetworkPassword);
 if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) { G->SelectedMode=SelectedMode; G->RoomHosting=false; G->bPlayChosen=true; }
 Deadline=FPlatformTime::Seconds()+60; SetStatus(ECultLobbyState::Connecting,TEXT("Connecting to host..."));
 UWidgetLayoutLibrary::RemoveAllWidgets(this);
 PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false;
 PC->ClientTravel(FString::Printf(TEXT("%s:%d?CultProtocol=1?CultMode=%d?CultKey=%s"),*HostAddress,HostPort,int32(SelectedMode),*JoiningInvite),TRAVEL_Absolute);
}
void UCultLobbySubsystem::HostGame(ECultPlayMode Mode,bool Private) {
 LeaveLobby(); SelectedMode=Mode; PrivateHost=Private; HostAddress=LocalRadminAddress();
 if(HostAddress.IsEmpty()) { SetStatus(ECultLobbyState::Error,TEXT("Start Radmin VPN before hosting a match.")); return; }
 if(NetworkName.Len()<3||NetworkPassword.Len()<6) { SetStatus(ECultLobbyState::Error,TEXT("Enter your Radmin network name and password before hosting.")); return; }
 SetStatus(ECultLobbyState::Creating,TEXT("Preparing your hosted match...")); EnsureSession([this] { CreateHost(); });
}
void UCultLobbySubsystem::CreateHost() {
 auto B=MakeShared<FJsonObject>(),C=MakeShared<FJsonObject>();
 B->SetStringField(TEXT("visibility"),PrivateHost?TEXT("private"):TEXT("public")); B->SetStringField(TEXT("mode"),WireMode(SelectedMode)); B->SetNumberField(TEXT("maxPlayers"),10);
 B->SetStringField(TEXT("region"),TEXT("sa")); B->SetStringField(TEXT("gameVersion"),TEXT("0.1.0")); B->SetStringField(TEXT("transport"),TEXT("radmin_desktop"));
 C->SetStringField(TEXT("networkName"),NetworkName); C->SetStringField(TEXT("networkPassword"),NetworkPassword); C->SetStringField(TEXT("hostAddress"),HostAddress); C->SetNumberField(TEXT("port"),7777); B->SetObjectField(TEXT("connection"),C);
 Request(TEXT("POST"),TEXT("/lobbies"),B,[this](int32 Code,FJson Json) {
  if(Code<200||Code>=300) { Fail(Code,Json); return; }
  LobbyId=String(Object(Json,TEXT("lobby")),TEXT("id")); HostToken=String(Json,TEXT("hostToken")); InviteCode=String(Json,TEXT("inviteCode"));
  if(!Identifier(LobbyId)||HostToken.IsEmpty()||(PrivateHost&&!Identifier(InviteCode))) { CleanupLate(Json); LobbyId.Empty(); HostToken.Empty(); Fail(0,nullptr); return; }
  if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) {
   G->SelectedMode=SelectedMode; G->RoomHosting=true; G->RoomPrivate=PrivateHost; G->RoomHost=HostAddress; G->RoomPort=7777; G->RoomKey=PrivateHost?InviteCode:FString(); G->bPlayChosen=true;
   G->RoomCode=PrivateHost?InviteCode:Site+TEXT("/join/")+LobbyId;
  }
  if(GetWorld()&&GetWorld()->GetNetMode()==NM_ListenServer) { Heartbeat(); return; }
  Deadline=FPlatformTime::Seconds()+90;
  UWidgetLayoutLibrary::RemoveAllWidgets(this);
  UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/L_DarkWilds"),true,FString::Printf(TEXT("listen?CultProtocol=1?CultMode=%d"),int32(SelectedMode)));
 });
}
void UCultLobbySubsystem::Heartbeat() {
 auto* W=GetWorld(); if(HostToken.IsEmpty()||!W||W->GetNetMode()!=NM_ListenServer||!W->GetNetDriver()||HeartbeatBusy) return;
 HeartbeatBusy=true;
 auto B=MakeShared<FJsonObject>(); B->SetBoolField(TEXT("sessionRunning"),true); B->SetBoolField(TEXT("inProgress"),MatchInProgress);
 Request(TEXT("POST"),TEXT("/lobbies/")+LobbyId+TEXT("/heartbeat"),B,[this](int32 Code,FJson Json) {
  HeartbeatBusy=false;
  if(Code==410) { HostToken.Empty(); LobbyId.Empty(); InviteCode.Empty(); CreateHost(); return; }
  if(Code<200||Code>=300) { SetStatus(ECultLobbyState::Hosting,TEXT("Your match is running; lobby listing could not refresh. Retrying at the next heartbeat.")); return; }
  SetStatus(ECultLobbyState::Hosting,TEXT("Your match is live. Share your invitation from the room menu."));
 },HostToken,false);
}
void UCultLobbySubsystem::SetMatchInProgress(bool InProgress) { MatchInProgress=InProgress; if(State==ECultLobbyState::Hosting) Heartbeat(); }
void UCultLobbySubsystem::ConfirmJoin() {
 ConfirmBusy=true;
 Request(TEXT("POST"),TEXT("/lobbies/")+LobbyId+TEXT("/join-confirm"),ReservationRef(ReservationId,ReservationToken),[this](int32 Code,FJson Json) {
  ConfirmBusy=false;
  if(Code>=200&&Code<300) { Deadline=0; RequiredNetworkPassword.Empty(); SetStatus(ECultLobbyState::Joined,TEXT("Connected to match.")); }
  else { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->ReturnToMenu(); else ReleaseReservation(); SetStatus(ECultLobbyState::Error,TEXT("Your lobby place could not be confirmed. Please join again.")); }
 },FString(),false);
}
void UCultLobbySubsystem::ReleaseReservation() {
 if(!ReservationId.IsEmpty()&&Identifier(LobbyId)) Cleanup(TEXT("/lobbies/")+LobbyId+TEXT("/leave"),ReservationRef(ReservationId,ReservationToken));
 ReservationId.Empty(); ReservationToken.Empty(); ReservationExpires=0; ConfirmBusy=false;
 if(HostToken.IsEmpty()) LobbyId.Empty();
}
void UCultLobbySubsystem::Cancel() { LeaveLobby(); }
void UCultLobbySubsystem::LeaveLobby() {
 const bool PendingTravel=State==ECultLobbyState::Connecting&&!ReservationId.IsEmpty();
 ++Generation; NextPoll=Deadline=0; PollFailures=0; SessionWaiters.Empty(); ConfirmBusy=HeartbeatBusy=false;
 if(auto* W=HeartbeatWorld.Get()) W->GetTimerManager().ClearTimer(HeartbeatTimer); HeartbeatWorld.Reset();
 ReleaseReservation(); if(!HostToken.IsEmpty()&&Identifier(LobbyId)) Cleanup(TEXT("/lobbies/")+LobbyId,nullptr,HostToken);
 if(PendingTravel&&GEngine&&GetWorld()) GEngine->CancelPending(GetWorld());
 HostToken.Empty(); LobbyId.Empty(); InviteCode.Empty(); JoiningInvite.Empty(); RequiredNetwork.Empty(); RequiredNetworkPassword.Empty(); MatchInProgress=false;
 SetStatus(ECultLobbyState::Idle,TEXT("Match search cancelled."));
}
bool UCultLobbySubsystem::Tick(float) {
 if(ShuttingDown) return false;
 const double Now=FPlatformTime::Seconds(); auto* W=GetWorld();
 if(State==ECultLobbyState::Searching&&NextPoll>0&&Now>=NextPoll) Poll();
 if(State==ECultLobbyState::WaitingForRadmin&&FDateTime::UtcNow().ToUnixTimestamp()>=ReservationExpires) { ReleaseReservation(); SetStatus(ECultLobbyState::Error,TEXT("Reserved place expired. Please search or join again.")); }
 if(Deadline>0&&Now>Deadline) { if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->ReturnToMenu(); else LeaveLobby(); SetStatus(ECultLobbyState::Error,TEXT("Connection timed out. Check that both players joined the same Radmin network.")); return true; }
 if(!W||W->bIsTearingDown) return true;
 if(!HostToken.IsEmpty()&&W->GetNetMode()==NM_ListenServer&&W->GetName().Contains(TEXT("L_DarkWilds"))&&W->GetNetDriver()&&HeartbeatWorld.Get()!=W) {
  // Never mark a pending lobby live just because OpenLevel was called; require the actual listen driver.
  FString Address,Port; W->GetNetDriver()->LowLevelGetNetworkNumber().Split(TEXT(":"),&Address,&Port,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
  if(FCString::Atoi(*Port)!=7777) { LeaveLobby(); SetStatus(ECultLobbyState::Error,TEXT("Online rooms must listen on port 7777. Restart without a custom port.")); return true; }
  if(auto* Old=HeartbeatWorld.Get()) Old->GetTimerManager().ClearTimer(HeartbeatTimer);
  HeartbeatWorld=W; Deadline=0; Heartbeat();
  W->GetTimerManager().SetTimer(HeartbeatTimer,this,&UCultLobbySubsystem::Heartbeat,60,true);
 }
 if(State==ECultLobbyState::Connecting&&!ReservationId.IsEmpty()&&!ConfirmBusy&&W->GetNetMode()==NM_Client) {
  auto* Driver=W->GetNetDriver(); auto* PC=W->GetFirstPlayerController();
  if(Driver&&Driver->ServerConnection&&Driver->ServerConnection->GetConnectionState()==USOCK_Open&&PC&&PC->GetPawn()&&W->GetName().Contains(TEXT("L_DarkWilds"))) ConfirmJoin();
 }
 return true;
}
void UCultLobbySubsystem::NetworkFailure(UWorld* W,UNetDriver*,ENetworkFailure::Type,const FString&) {
 if(W&&W->GetGameInstance()!=GetGameInstance()) return;
 if(HasActiveLobby()) { LeaveLobby(); SetStatus(ECultLobbyState::Error,TEXT("Host disconnected or connection failed. Check your shared Radmin network.")); }
}
void UCultLobbySubsystem::TravelFailure(UWorld* W,ETravelFailure::Type,const FString&) {
 if(W&&W->GetGameInstance()!=GetGameInstance()) return;
 if(HasActiveLobby()) { LeaveLobby(); SetStatus(ECultLobbyState::Error,TEXT("Could not load the match. Your reserved place has been released.")); }
}
