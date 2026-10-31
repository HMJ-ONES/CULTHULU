#include "CultRoomService.h"
#include "CultUlhuGameInstance.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "Common/UdpSocketBuilder.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
FCultRoomService::FCultRoomService(UCultUlhuGameInstance* Owner):Game(Owner) {
 FParse::Value(FCommandLine::Get(),TEXT("CultDiscoveryPort="),DiscoveryPort); DiscoveryPort=FMath::Clamp(DiscoveryPort,1024,65535);
 Handle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this,&FCultRoomService::Tick),.1f);
}
FCultRoomService::~FCultRoomService() { FTSTicker::GetCoreTicker().RemoveTicker(Handle); Close(HostSocket); Close(SearchSocket); }
void FCultRoomService::Close(FSocket*& Socket) { if(Socket) { Socket->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket); Socket=nullptr; } }
void FCultRoomService::StopSearch() { Close(SearchSocket); SearchEnds=0; }
void FCultRoomService::Find() {
 StopSearch(); SearchSocket=FUdpSocketBuilder(TEXT("CultRoomSearch")).AsNonBlocking().WithBroadcast().BoundToPort(0).WithReceiveBufferSize(8192);
 if(!SearchSocket) { Game->RoomStatus=TEXT("Room search could not start. Try Enter Room Code."); return; }
 Nonce=FGuid::NewGuid().A; SearchEnds=FPlatformTime::Seconds()+3; NextQuery=0; Game->RoomStatus=TEXT("Looking for an open room on your Radmin / LAN network...");
}
bool FCultRoomService::Tick(float) {
 auto* World=Game->GetWorld(); const double Now=FPlatformTime::Seconds();
 const bool Hosting=Game->RoomHosting&&!Game->RoomPrivate&&World&&World->GetNetMode()==NM_ListenServer;
 if(Game->RoomHosting&&World&&World->GetNetMode()==NM_ListenServer) {
  if(auto* Driver=World->GetNetDriver()) { FString Left,Right; if(Driver->LowLevelGetNetworkNumber().Split(TEXT(":"),&Left,&Right,ESearchCase::CaseSensitive,ESearchDir::FromEnd)&&Right.IsNumeric()) { const int Port=FCString::Atoi(*Right); if(Port>=1024&&Port<=65535) Game->RoomPort=Port; } }
  FCultRoomAddress R{Game->RoomHost,Game->RoomKey,uint16(Game->RoomPort),Game->SelectedMode,Game->RoomPrivate}; Game->RoomCode=CultRoomCode::Encode(R);
 }
 if(Hosting&&!HostSocket) HostSocket=FUdpSocketBuilder(TEXT("CultPublicRoom")).AsReusable().AsNonBlocking().WithBroadcast().BoundToPort(DiscoveryPort).WithReceiveBufferSize(8192);
 if(!Hosting) Close(HostSocket);
 auto Send=[&](FSocket* Socket,const FString& Text,const TSharedRef<FInternetAddr>& Address) { FTCHARToUTF8 Bytes(*Text); int Sent=0; Socket->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length(),Sent,*Address); };
 if(SearchSocket&&Now>=NextQuery&&Now<SearchEnds) {
  NextQuery=Now+.65; TArray<FString> Targets={TEXT("255.255.255.255"),TEXT("127.0.0.1")};
  TArray<TSharedPtr<FInternetAddr>> Adapters; ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(Adapters);
  for(auto& A:Adapters) { const FString IP=A->ToString(false); TArray<FString> Octets; IP.ParseIntoArray(Octets,TEXT(".")); if(Octets.Num()==4) { Targets.AddUnique(Octets[0]+TEXT(".")+Octets[1]+TEXT(".")+Octets[2]+TEXT(".255")); if(IP.StartsWith(TEXT("26."))) Targets.AddUnique(TEXT("26.255.255.255")); } }
  for(const auto& IP:Targets) { auto Address=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr(); bool Valid=false; Address->SetIp(*IP,Valid); Address->SetPort(DiscoveryPort); if(Valid) Send(SearchSocket,FString::Printf(TEXT("CULTFIND1|%d|%u"),int(Game->SelectedMode),Nonce),Address); }
 }
 for(int Kind=0;Kind<2;++Kind) {
  FSocket* Socket=Kind?SearchSocket:HostSocket; if(!Socket) continue;
  uint32 Pending=0; for(int Packet=0;Packet<32&&Socket->HasPendingData(Pending);++Packet) {
   uint8 Data[1025]={}; int Read=0; auto From=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
   if(!Socket->RecvFrom(Data,1024,Read,*From)||Read<=0||Read>=1024) continue; Data[Read]=0;
   TArray<FString> Fields; FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data))).ParseIntoArray(Fields,TEXT("|"),false);
   if(!Kind&&Fields.Num()==3&&Fields[0]==TEXT("CULTFIND1")&&Fields[1]==FString::FromInt(int(Game->SelectedMode))&&Fields[2].Len()<=10&&Fields[2].IsNumeric()) {
    int Players=0; for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It) if(It->Get()&&It->Get()->GetPawn()) ++Players;
    const int Limit=Game->SelectedMode==ECultPlayMode::FreeRoam?8:10;
    if(Players<Limit) Send(Socket,FString::Printf(TEXT("CULTROOM1|%d|%d|%d|%d|%s|1"),int(Game->SelectedMode),Game->RoomPort,Players,Limit,*Fields[2]),From);
   } else if(Kind&&Fields.Num()==7&&Fields[0]==TEXT("CULTROOM1")&&Fields[1]==FString::FromInt(int(Game->SelectedMode))&&Fields[5]==FString::Printf(TEXT("%u"),Nonce)&&Fields[6]==TEXT("1")) {
    if(!Fields[2].IsNumeric()||!Fields[3].IsNumeric()||!Fields[4].IsNumeric()) continue;
    const int Port=FCString::Atoi(*Fields[2]),Players=FCString::Atoi(*Fields[3]),Limit=FCString::Atoi(*Fields[4]); if(Port<1024||Port>65535||Players<0||Players>=Limit||Limit>10) continue;
    const FString Host=From->ToString(false); StopSearch(); Game->PublicRoomFound(Host,Port); return true;
   }
  }
 }
 if(SearchSocket&&Now>=SearchEnds) { StopSearch(); Game->RoomAction=ECultRoomAction::None; Game->RoomStatus=TEXT("No open room found. Choose Enter / Host, or paste your friend's room code."); }
 return true;
}
