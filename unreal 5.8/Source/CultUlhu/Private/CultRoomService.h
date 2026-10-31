#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
class UCultUlhuGameInstance;
class FSocket;
enum class ECultPlayMode:uint8;
struct FCultRoomAddress {
 FString Host,Key; uint16 Port=7777; ECultPlayMode Mode=static_cast<ECultPlayMode>(0); bool Private=false;
};
namespace CultRoomCode {
 FString Encode(const FCultRoomAddress& Room);
 bool Decode(FString Code,FCultRoomAddress& Room,FString& Error);
 FString LocalAddress();
}
class FCultRoomService {
 UCultUlhuGameInstance* Game; FSocket* HostSocket=nullptr; FSocket* SearchSocket=nullptr; FTSTicker::FDelegateHandle Handle;
 double SearchEnds=0,NextQuery=0; uint32 Nonce=0; int DiscoveryPort=27777;
 bool Tick(float);
 void Close(FSocket*& Socket);
public:
 explicit FCultRoomService(UCultUlhuGameInstance* Owner);
 ~FCultRoomService();
 void Find();
 void StopSearch();
};
