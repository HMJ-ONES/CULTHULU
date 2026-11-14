#include "CultRoomService.h"
#include "CultUlhuGameInstance.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Misc/Crc.h"
namespace {
const TCHAR* Alphabet=TEXT("0123456789ABCDEFGHJKMNPQRSTVWXYZ");
bool IPv4(const FString& Text,uint32& Value) {
 TArray<FString> Parts; Text.ParseIntoArray(Parts,TEXT("."),false); if(Parts.Num()!=4) return false; Value=0;
 for(const auto& Part:Parts) { if(Part.IsEmpty()||Part.Len()>3||!Part.IsNumeric()) return false; const int N=FCString::Atoi(*Part); if(N<0||N>255) return false; Value=(Value<<8)|N; }
 return (Value>>24)>0&&(Value>>24)<224;
}
}
FString CultRoomCode::Encode(const FCultRoomAddress& R) {
 uint32 IP=0; if(!IPv4(R.Host,IP)||R.Port<1024||int(R.Mode)>2) return FString();
 if(R.Private) { if(R.Key.Len()!=16) return FString(); for(TCHAR C:R.Key) if(!FChar::IsHexDigit(C)) return FString(); }
 TArray<uint8> Data; Data.Add(uint8(R.Mode)|(R.Private?0x80:0));
 for(int I=3;I>=0;--I) Data.Add(uint8(IP>>(I*8))); Data.Add(R.Port>>8); Data.Add(uint8(R.Port));
 const uint64 Key=R.Private?FCString::Strtoui64(*R.Key,nullptr,16):0; if(R.Private&&Key==0) return FString();
 for(int I=7;I>=0;--I) Data.Add(uint8(Key>>(I*8)));
 const uint32 CRC=FCrc::MemCrc32(Data.GetData(),Data.Num()); for(int I=3;I>=0;--I) Data.Add(uint8(CRC>>(I*8)));
 FString Encoded; uint32 Bits=0; int Count=0;
 for(uint8 Byte:Data) { Bits=(Bits<<8)|Byte; Count+=8; while(Count>=5) { Count-=5; Encoded.AppendChar(Alphabet[(Bits>>Count)&31]); } }
 if(Count) Encoded.AppendChar(Alphabet[(Bits<<(5-Count))&31]);
 FString Code=TEXT("CU1"); for(int I=0;I<Encoded.Len();++I) { if(I%5==0) Code+=TEXT("-"); Code.AppendChar(Encoded[I]); } return Code;
}
bool CultRoomCode::Decode(FString Code,FCultRoomAddress& R,FString& Error) {
 Error=TEXT("That room code is invalid. Copy the complete code from your host."); if(Code.Len()>120) return false;
 Code=Code.ToUpper(); Code.ReplaceInline(TEXT("-"),TEXT("")); Code.ReplaceInline(TEXT(" "),TEXT("")); Code.ReplaceInline(TEXT("\r"),TEXT("")); Code.ReplaceInline(TEXT("\n"),TEXT(""));
 if(!Code.StartsWith(TEXT("CU1"))||Code.Len()!=34) return false; Code=Code.Mid(3);
 TArray<uint8> Data; uint32 Bits=0; int Count=0;
 for(TCHAR C:Code) { if(C=='O') C='0'; if(C=='I'||C=='L') C='1'; const TCHAR* Found=FCString::Strchr(Alphabet,C); if(!Found) return false; Bits=(Bits<<5)|int(Found-Alphabet); Count+=5; if(Count>=8) { Count-=8; Data.Add(uint8(Bits>>Count)); } }
 if(Data.Num()!=19||(Bits&((1u<<Count)-1))!=0) return false;
 uint32 CRC=0; for(int I=15;I<19;++I) CRC=(CRC<<8)|Data[I]; if(CRC!=FCrc::MemCrc32(Data.GetData(),15)||(Data[0]&0x7c)||((Data[0]&3)>2)) return false;
 R.Mode=ECultPlayMode(Data[0]&3); R.Private=(Data[0]&0x80)!=0; R.Host=FString::Printf(TEXT("%d.%d.%d.%d"),Data[1],Data[2],Data[3],Data[4]); R.Port=(uint16(Data[5])<<8)|Data[6];
 uint32 IP; if(!IPv4(R.Host,IP)||R.Port<1024) return false; uint64 Key=0; for(int I=7;I<15;++I) Key=(Key<<8)|Data[I];
 if((R.Private&&Key==0)||(!R.Private&&Key!=0)) return false;
 R.Key=R.Private?FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(Key)):FString(); Error.Empty(); return true;
}
FString CultRoomCode::LocalAddress() {
 TArray<TSharedPtr<FInternetAddr>> Addresses; ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(Addresses); FString LAN;
 for(auto& Address:Addresses) { const FString IP=Address->ToString(false); uint32 V; if(!IPv4(IP,V)||IP.StartsWith(TEXT("127."))||IP.StartsWith(TEXT("169.254."))) continue; if(IP.StartsWith(TEXT("26."))) return IP; if(LAN.IsEmpty()) LAN=IP; }
 return LAN.IsEmpty()?TEXT("127.0.0.1"):LAN;
}
