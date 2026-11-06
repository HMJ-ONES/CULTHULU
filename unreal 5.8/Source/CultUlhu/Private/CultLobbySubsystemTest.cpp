#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultLobbySubsystem.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultLobbyInputTest,"CultUlhu.Network.CloudLobby.InputBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultLobbyInputTest::RunTest(const FString&) {
 FString Id,Invite;
 for(const FString& S:TArray<FString>{TEXT("lby_Ab3dE6gH9"),TEXT("https://cultulhu-lobby.pages.dev/join/lby_Ab3dE6gH9"),TEXT("cultulhu://join/lby_Ab3dE6gH9"),TEXT("42aa6e66-235e-4d2e-8c43-e82b37416812")}) {
  TestTrue(TEXT("Supported identifier/link"),UCultLobbySubsystem::ParseLobbyInput(S,Id,Invite)); TestFalse(TEXT("ID extracted"),Id.IsEmpty());
 }
 TestTrue(TEXT("Private invitation accepted"),UCultLobbySubsystem::ParseLobbyInput(TEXT("ABCD-EFGH-JKLM"),Id,Invite)); TestTrue(TEXT("Invite uses resolver"),Id.IsEmpty()&&!Invite.IsEmpty());
 for(const FString& S:TArray<FString>{TEXT("https://evil.test/join/lby_Ab3dE6gH9"),TEXT("https://cultulhu-lobby.pages.dev.evil.test/join/lby_Ab3dE6gH9"),TEXT("lby_12345678?exec=quit"),TEXT("cultulhu://join/../../exec"),TEXT("open 26.1.2.3:7777|quit"),TEXT("lby_12345678\nquit")}) TestFalse(TEXT("Command/foreign origin rejected"),UCultLobbySubsystem::ParseLobbyInput(S,Id,Invite));
 for(const FString& S:TArray<FString>{TEXT("26.1.2.3"),TEXT("26.33.200.70")}) TestTrue(TEXT("Strict Radmin IPv4"),UCultLobbySubsystem::IsRadminAddress(S));
 for(const FString& S:TArray<FString>{TEXT("127.0.0.1"),TEXT("26.256.1.2"),TEXT("26.1.2.3?listen"),TEXT("26.1.2.3:7777"),TEXT("26.01.2.3"),TEXT("26.1.2.3\nquit"),TEXT("26.255.255.255")}) TestFalse(TEXT("Unsupported destination rejected"),UCultLobbySubsystem::IsRadminAddress(S));
 TestEqual(TEXT("DOTGOO canonical mode"),UCultLobbySubsystem::WireMode(ECultPlayMode::Dotgoo),FString(TEXT("dotgoo")));
 return true;
}
#endif
