#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CultUlhuGameInstance.h"
#include "Containers/Ticker.h"
#include "CultLobbySubsystem.generated.h"

class FJsonObject;
UENUM(BlueprintType)
enum class ECultLobbyState : uint8 { Idle, Authenticating, Searching, Creating, WaitingForRadmin, Connecting, Hosting, Joined, Error };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCultLobbyStatusChanged, ECultLobbyState, State, const FString&, Message);

/** Production lobby directory client. Gameplay packets still use UE replication over Radmin.
 * Credentials are memory-only. No response bodies, authorization headers or invitation codes are logged.
 */
UCLASS()
class CULTULHU_API UCultLobbySubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 UCultLobbySubsystem();
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void QuickPlay(ECultPlayMode Mode, bool OpenBrowser=true);
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void HostGame(ECultPlayMode Mode, bool Private=false);
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void JoinByCode(const FString& Input, ECultPlayMode Mode);
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void Cancel();
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void LeaveLobby();
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void BrowseLobbies(ECultPlayMode Mode);
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void ContinueRadminConnection();
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") bool ConfigureRadminNetwork(const FString& Name, const FString& Password);
 UFUNCTION(BlueprintCallable, Category="CultUlhu|Lobby") void SetMatchInProgress(bool InProgress);
 UFUNCTION(BlueprintPure, Category="CultUlhu|Lobby") static FString LocalRadminAddress();
 UPROPERTY(BlueprintAssignable) FCultLobbyStatusChanged OnStatusChanged;
 UPROPERTY(BlueprintReadOnly) ECultLobbyState State=ECultLobbyState::Idle;
 UPROPERTY(BlueprintReadOnly) FString Status;
 UPROPERTY(BlueprintReadOnly) FString LobbyId;
 UPROPERTY(BlueprintReadOnly) FString InviteCode;
 UPROPERTY(BlueprintReadOnly) FString RequiredNetwork;
 // Only displayed in the dedicated connection panel; never included in status/log messages.
 UPROPERTY(BlueprintReadOnly, Transient) FString RequiredNetworkPassword;
 UPROPERTY(BlueprintReadOnly) FString PlayerId;
 bool HasActiveLobby() const { return !LobbyId.IsEmpty(); }
 static bool ParseLobbyInput(const FString& Input,FString& Id,FString& PrivateInvite);
 static bool IsRadminAddress(const FString& Address);
 static FString WireMode(ECultPlayMode Mode);
private:
 using FJson=TSharedPtr<FJsonObject>;
 using FReply=TFunction<void(int32,FJson)>;
 void Request(const FString& Verb,const FString& Path,FJson Body,FReply Reply,const FString& HostCredential=FString(),bool Authenticate=true);
 void EnsureSession(TFunction<void()> Ready);
 void Poll();
 void Reserve(const FString& Id,const FString& Code);
 void AcceptReservation(FJson Json);
 void CreateHost();
 void Heartbeat();
 void ConfirmJoin();
 void ReleaseReservation();
 void SetStatus(ECultLobbyState NewState,const FString& Message);
 void Fail(int32 Code,FJson Json);
 bool Tick(float Delta);
 bool Compatible(FJson Lobby,bool RequireJoinable=true) const;
 void NetworkFailure(UWorld*,UNetDriver*,ENetworkFailure::Type,const FString&);
 void TravelFailure(UWorld*,ETravelFailure::Type,const FString&);
 FString SessionToken,HostToken,ReservationId,ReservationToken,HostAddress;
 FString NetworkName=TEXT("CULT-ULHU-PUBLIC"),NetworkPassword;
 FString JoiningInvite;
 TSet<FString> AcknowledgedNetworks;
 ECultPlayMode SelectedMode=ECultPlayMode::FreeRoam;
 bool PrivateHost=false,MatchInProgress=false,SessionBusy=false,ConfirmBusy=false,HeartbeatBusy=false,ShuttingDown=false;
 int32 Generation=0,PollFailures=0;
 double SessionExpires=0,NextPoll=0,Deadline=0,ReservationExpires=0;
 int32 HostPort=7777;
 TArray<TFunction<void()>> SessionWaiters;
 FTSTicker::FDelegateHandle Ticker;
 FTimerHandle HeartbeatTimer;
 TWeakObjectPtr<UWorld> HeartbeatWorld;
};
