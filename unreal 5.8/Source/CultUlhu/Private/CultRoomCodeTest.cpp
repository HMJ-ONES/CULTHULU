#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultRoomService.h"
#include "CultUlhuGameInstance.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultRoomCodeTest,"CultUlhu.Network.RoomCodes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultRoomCodeTest::RunTest(const FString&) {
 for(int Mode=0;Mode<3;++Mode) for(bool Private:{false,true}) {
  FCultRoomAddress A{TEXT("26.123.45.67"),Private?TEXT("123456789ABCDEF0"):TEXT(""),7791,ECultPlayMode(Mode),Private},B; FString Error;
  const FString Code=CultRoomCode::Encode(A);
  TestTrue(TEXT("Code round trip"),CultRoomCode::Decode(Code.ToLower(),B,Error)); TestEqual(TEXT("Mode preserved"),int(B.Mode),Mode); TestEqual(TEXT("Host preserved"),B.Host,A.Host); TestEqual(TEXT("Port preserved"),int(B.Port),7791); TestEqual(TEXT("Privacy preserved"),B.Private,Private); TestEqual(TEXT("Key preserved"),B.Key,A.Key);
  FString Damaged=Code; Damaged[5]=Damaged[5]=='A'?'B':'A'; TestFalse(TEXT("Damaged code rejected"),CultRoomCode::Decode(Damaged,B,Error));
  auto* G=NewObject<UCultUlhuGameInstance>(); G->RoomHosting=true; G->SelectedMode=A.Mode; G->RoomPrivate=Private; G->RoomKey=A.Key;
  Error.Empty(); G->ValidateRoomLogin(FString::Printf(TEXT("?CultProtocol=1?CultMode=%d?CultKey=%s"),Mode,*A.Key),Error); TestTrue(TEXT("Correct invitation accepted"),Error.IsEmpty());
  Error.Empty(); G->ValidateRoomLogin(FString::Printf(TEXT("?CultProtocol=1?CultMode=%d?CultKey=0000000000000001"),Mode),Error); TestEqual(TEXT("Private invitations require their secret"),!Error.IsEmpty(),Private);
  Error.Empty(); G->ValidateRoomLogin(TEXT("?CultProtocol=0?CultMode=0"),Error); TestFalse(TEXT("Wrong protocol rejected"),Error.IsEmpty());
 }
 FCultRoomAddress A,B; FString Error; A.Host=TEXT("127.0.0.1?listen"); TestTrue(TEXT("Travel options cannot be encoded"),CultRoomCode::Encode(A).IsEmpty());
 A.Host=TEXT("127.0.0.1"); A.Port=80; TestTrue(TEXT("Reserved ports rejected"),CultRoomCode::Encode(A).IsEmpty()); A.Port=7777; A.Private=true; A.Key=TEXT("0000000000000000"); TestTrue(TEXT("Empty private secret rejected"),CultRoomCode::Encode(A).IsEmpty());
 TestFalse(TEXT("Arbitrary text cannot join"),CultRoomCode::Decode(TEXT("open 127.0.0.1"),B,Error)); return true;
}
#endif
