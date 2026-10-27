#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TimerHandle.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MallMultiplayerSubsystem.generated.h"

class UNetDriver;

USTRUCT(BlueprintType)
struct FMallSessionListing
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") int32 Index = 0;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") FString HostName;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") int32 PingMilliseconds = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMallSessionStatusChanged, bool, bSuccess, const FString&, Message);

/** LAN now, EOS lobbies when the project's default online subsystem is configured for EOS. */
UCLASS()
class MYPROJECT2_API UMallMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Mall|Multiplayer")
	bool HostSession(int32 MaxPlayers = 0);

	UFUNCTION(BlueprintCallable, Category="Mall|Multiplayer")
	bool FindSessions();

	UFUNCTION(BlueprintCallable, Category="Mall|Multiplayer")
	bool JoinSession(int32 ResultIndex);

	UFUNCTION(BlueprintCallable, Category="Mall|Multiplayer")
	bool LeaveSession();

	/** Explicit interactive Epic-account login; never called automatically in PIE. */
	UFUNCTION(BlueprintCallable, Category="Mall|Multiplayer")
	bool LoginEOS();

	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") FString Status = TEXT("Ready (local play)");
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") FString BackendName;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") bool bBusy = false;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") int32 HostedMaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Mall|Multiplayer") TArray<FMallSessionListing> Results;
	UPROPERTY(BlueprintAssignable, Category="Mall|Multiplayer") FMallSessionStatusChanged OnStatusChanged;

private:
	enum class EOperation : uint8 { Idle, LoggingIn, Creating, Finding, Joining, Leaving };
	enum class ERequest : uint8 { None, Host, Find, Join };
	enum class EReady : uint8 { Failed, PendingLogin, Ready };

	EReady EnsureReady(ERequest Request);
	bool AcquireBackend();
	int32 LocalUserIndex() const;
	bool IsEOS() const;
	void SetStatus(bool bSuccess, const FString& Message);
	void Finish(bool bSuccess, const FString& Message);
	void SetOperationTimeout();
	void OperationTimedOut();
	void ClearOperationDelegates();
	void ReleaseBackend();
	void ReturnToStandalone();
	void CleanupFailedConnection(const FString& Message);

	void OnCreateComplete(FName SessionName, bool bSuccess);
	void OnFindComplete(bool bSuccess);
	void OnJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroyComplete(FName SessionName, bool bSuccess);
	void OnLoginComplete(int32 UserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error);
	void OnLoginStatusChanged(int32 UserNum, ELoginStatus::Type Previous, ELoginStatus::Type Current, const FUniqueNetId& UserId);
	void OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);

	IOnlineSessionPtr Sessions;
	IOnlineIdentityPtr Identity;
	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> JoinableResults;
	EOperation Operation = EOperation::Idle;
	ERequest PendingRequest = ERequest::None;
	int32 PendingMaxPlayers = 4;
	int32 PendingJoinIndex = INDEX_NONE;
	int32 BoundLocalUserIndex = 0;
	bool bLeaveAfterPendingOperation = false;
	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle, LoginHandle, LoginStatusHandle;
	FDelegateHandle NetworkFailureHandle, TravelFailureHandle;
	FTimerHandle OperationTimer;
};
