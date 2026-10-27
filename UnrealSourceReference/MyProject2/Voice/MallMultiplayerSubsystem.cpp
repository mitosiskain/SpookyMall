#include "Voice/MallMultiplayerSubsystem.h"

#include "Voice/MallVoiceSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMallMultiplayer, Log, All);

namespace MallMultiplayer
{
	const TCHAR* TestMap = TEXT("/Game/Environment/Mall/Maps/Lvl_MallOval_Test");
	const FName ProtocolKey(TEXT("MALL_PROTOCOL"));
	const FString ProtocolValue(TEXT("MallVoiceV1"));

	UMallMultiplayerSubsystem* FromWorld(UWorld* World)
	{
		return World && World->IsGameWorld() && World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UMallMultiplayerSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs HostCommand(TEXT("mall.Host"), TEXT("Host the mall test map; optional total player capacity (default 4)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMallMultiplayerSubsystem* Subsystem = FromWorld(World)) Subsystem->HostSession(Args.IsEmpty() ? 0 : FCString::Atoi(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs FindCommand(TEXT("mall.Find"), TEXT("Find compatible LAN sessions, or EOS lobbies when EOS is configured."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMallMultiplayerSubsystem* Subsystem = FromWorld(World)) Subsystem->FindSessions();
		}));
	FAutoConsoleCommandWithWorldAndArgs JoinCommand(TEXT("mall.Join"), TEXT("Join a numbered result from mall.Find (zero based)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMallMultiplayerSubsystem* Subsystem = FromWorld(World)) Subsystem->JoinSession(Args.IsEmpty() ? 0 : FCString::Atoi(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs LeaveCommand(TEXT("mall.Leave"), TEXT("Leave the current mall session and return to local play."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMallMultiplayerSubsystem* Subsystem = FromWorld(World)) Subsystem->LeaveSession();
		}));
	FAutoConsoleCommandWithWorldAndArgs LoginCommand(TEXT("mall.Login"), TEXT("Explicit Epic-account portal login, only when EOS is configured."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMallMultiplayerSubsystem* Subsystem = FromWorld(World)) Subsystem->LoginEOS();
		}));
}

void UMallMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UMallMultiplayerSubsystem::OnNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UMallMultiplayerSubsystem::OnTravelFailure);
	}
}

void UMallMultiplayerSubsystem::Deinitialize()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(OperationTimer);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (Sessions.IsValid())
	{
		const bool bWasFinding = Operation == EOperation::Finding;
		ClearOperationDelegates();
		if (bWasFinding) Sessions->CancelFindSessions();
		if (Sessions->GetNamedSession(NAME_GameSession)) Sessions->DestroySession(NAME_GameSession);
	}
	ReleaseBackend();
	Super::Deinitialize();
}

int32 UMallMultiplayerSubsystem::LocalUserIndex() const
{
	const ULocalPlayer* Player = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	return Player ? FMath::Max(0, Player->GetControllerId()) : 0;
}

bool UMallMultiplayerSubsystem::IsEOS() const
{
	return BackendName.Equals(TEXT("EOS"), ESearchCase::IgnoreCase);
}

bool UMallMultiplayerSubsystem::AcquireBackend()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		SetStatus(false, TEXT("Start the game or PIE before using multiplayer commands."));
		return false;
	}
	IOnlineSubsystem* Backend = Online::GetSubsystem(World);
	IOnlineSessionPtr NewSessions = Backend ? Backend->GetSessionInterface() : nullptr;
	if (!Backend || !NewSessions.IsValid())
	{
		SetStatus(false, TEXT("No online session backend is available. Keep NULL for LAN, or configure EOS first."));
		return false;
	}
	if (Sessions != NewSessions)
	{
		ReleaseBackend();
		Sessions = NewSessions;
		Identity = Backend->GetIdentityInterface();
		BackendName = Backend->GetSubsystemName().ToString();
		BoundLocalUserIndex = LocalUserIndex();
		if (Identity.IsValid())
		{
			LoginStatusHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(BoundLocalUserIndex,
				FOnLoginStatusChangedDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnLoginStatusChanged));
		}
	}
	return true;
}

UMallMultiplayerSubsystem::EReady UMallMultiplayerSubsystem::EnsureReady(const ERequest Request)
{
	if (!AcquireBackend()) return EReady::Failed;
	if (!IsEOS()) return EReady::Ready;
	if (!Identity.IsValid())
	{
		SetStatus(false, TEXT("EOS identity is unavailable. Configure the EOS artifact and identity settings first."));
		return EReady::Failed;
	}
	if (Identity->GetLoginStatus(BoundLocalUserIndex) == ELoginStatus::LoggedIn) return EReady::Ready;
	Operation = EOperation::LoggingIn;
	bBusy = true;
	PendingRequest = Request;
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(BoundLocalUserIndex,
		FOnLoginCompleteDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnLoginComplete));
	SetStatus(true, TEXT("Signing into EOS using the configured platform/developer credentials..."));
	SetOperationTimeout();
	if (!Identity->AutoLogin(BoundLocalUserIndex) && Operation == EOperation::LoggingIn)
	{
		Finish(false, TEXT("EOS automatic login could not start. Configure credentials or use mall.Login for an explicit Epic-account login."));
		return EReady::Failed;
	}
	return EReady::PendingLogin;
}

bool UMallMultiplayerSubsystem::LoginEOS()
{
	if (bBusy || !AcquireBackend()) return false;
	if (!IsEOS() || !Identity.IsValid())
	{
		SetStatus(false, TEXT("EOS is not active. Current local/LAN play needs no account login."));
		return false;
	}
	if (Identity->GetLoginStatus(BoundLocalUserIndex) == ELoginStatus::LoggedIn)
	{
		SetStatus(true, TEXT("Already signed into EOS."));
		return true;
	}
	Operation = EOperation::LoggingIn;
	bBusy = true;
	PendingRequest = ERequest::None;
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(BoundLocalUserIndex,
		FOnLoginCompleteDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnLoginComplete));
	SetOperationTimeout();
	SetStatus(true, TEXT("Opening the Epic-account sign-in flow..."));
	if (!Identity->Login(BoundLocalUserIndex, FOnlineAccountCredentials(TEXT("accountportal"), FString(), FString()))
		&& Operation == EOperation::LoggingIn)
	{
		Finish(false, TEXT("Epic-account login could not start. Check EOS configuration and account services."));
		return false;
	}
	return true;
}

bool UMallMultiplayerSubsystem::HostSession(const int32 MaxPlayers)
{
	if (bBusy) { SetStatus(false, TEXT("A multiplayer operation is already running.")); return false; }
	PendingMaxPlayers = FMath::Clamp(MaxPlayers > 0 ? MaxPlayers : GetDefault<UMallVoiceSettings>()->DefaultMaxPlayers, 1, 64);
	const EReady Ready = EnsureReady(ERequest::Host);
	if (Ready != EReady::Ready) return Ready == EReady::PendingLogin;
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		SetStatus(false, TEXT("A session is already active. Use mall.Leave before hosting another."));
		return false;
	}
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = PendingMaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = BackendName.Equals(TEXT("NULL"), ESearchCase::IgnoreCase);
	Settings.bIsDedicated = false;
	Settings.bUsesPresence = IsEOS();
	Settings.bAllowInvites = IsEOS();
	Settings.bAllowJoinViaPresence = IsEOS();
	Settings.bUseLobbiesIfAvailable = IsEOS();
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.Set(SETTING_MAPNAME, FString(MallMultiplayer::TestMap), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(MallMultiplayer::ProtocolKey, MallMultiplayer::ProtocolValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Operation = EOperation::Creating;
	bBusy = true;
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnCreateComplete));
	SetOperationTimeout();
	SetStatus(true, FString::Printf(TEXT("Creating %s session for %d players..."), *BackendName, PendingMaxPlayers));
	if (!Sessions->CreateSession(BoundLocalUserIndex, NAME_GameSession, Settings) && Operation == EOperation::Creating)
	{
		Finish(false, TEXT("The session backend rejected the host request."));
		return false;
	}
	return true;
}

bool UMallMultiplayerSubsystem::FindSessions()
{
	if (bBusy) { SetStatus(false, TEXT("A multiplayer operation is already running.")); return false; }
	const EReady Ready = EnsureReady(ERequest::Find);
	if (Ready != EReady::Ready) return Ready == EReady::PendingLogin;
	Results.Reset();
	JoinableResults.Reset();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 50;
	Search->bIsLanQuery = BackendName.Equals(TEXT("NULL"), ESearchCase::IgnoreCase);
	Search->TimeoutInSeconds = 20.0;
	Search->QuerySettings.Set(MallMultiplayer::ProtocolKey, MallMultiplayer::ProtocolValue, EOnlineComparisonOp::Equals);
	if (IsEOS()) Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Operation = EOperation::Finding;
	bBusy = true;
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnFindComplete));
	SetOperationTimeout();
	SetStatus(true, FString::Printf(TEXT("Searching %s for mall sessions..."), *BackendName));
	if (!Sessions->FindSessions(BoundLocalUserIndex, Search.ToSharedRef()) && Operation == EOperation::Finding)
	{
		Finish(false, TEXT("The session backend rejected the search request."));
		return false;
	}
	return true;
}

bool UMallMultiplayerSubsystem::JoinSession(const int32 ResultIndex)
{
	if (bBusy) { SetStatus(false, TEXT("A multiplayer operation is already running.")); return false; }
	PendingJoinIndex = ResultIndex;
	const EReady Ready = EnsureReady(ERequest::Join);
	if (Ready != EReady::Ready) return Ready == EReady::PendingLogin;
	if (!JoinableResults.IsValidIndex(ResultIndex))
	{
		SetStatus(false, TEXT("Invalid session index. Run mall.Find, then mall.Join with a listed index."));
		return false;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		SetStatus(false, TEXT("A session is already active. Use mall.Leave before joining another."));
		return false;
	}
	Operation = EOperation::Joining;
	bBusy = true;
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnJoinComplete));
	SetOperationTimeout();
	SetStatus(true, FString::Printf(TEXT("Joining session %d..."), ResultIndex));
	if (!Sessions->JoinSession(BoundLocalUserIndex, NAME_GameSession, JoinableResults[ResultIndex]) && Operation == EOperation::Joining)
	{
		Finish(false, TEXT("The session backend rejected the join request."));
		return false;
	}
	return true;
}

bool UMallMultiplayerSubsystem::LeaveSession()
{
	if (Operation == EOperation::Creating || Operation == EOperation::Joining)
	{
		bLeaveAfterPendingOperation = true;
		SetStatus(true, TEXT("Leaving as soon as the pending session operation completes..."));
		return true;
	}
	if (Operation == EOperation::Leaving) return true;
	if (Operation == EOperation::Finding && Sessions.IsValid()) Sessions->CancelFindSessions();
	ClearOperationDelegates();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(OperationTimer);
	Operation = EOperation::Idle;
	bBusy = false;
	PendingRequest = ERequest::None;
	if (!AcquireBackend()) { ReturnToStandalone(); return false; }
	if (!Sessions->GetNamedSession(NAME_GameSession))
	{
		HostedMaxPlayers = 0;
		SetStatus(true, TEXT("Local play; no active session."));
		ReturnToStandalone();
		return true;
	}
	Operation = EOperation::Leaving;
	bBusy = true;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMallMultiplayerSubsystem::OnDestroyComplete));
	SetOperationTimeout();
	SetStatus(true, TEXT("Leaving the session..."));
	if (!Sessions->DestroySession(NAME_GameSession) && Operation == EOperation::Leaving)
	{
		Sessions->RemoveNamedSession(NAME_GameSession);
		Finish(false, TEXT("The backend could not destroy the session; returning to local play."));
		ReturnToStandalone();
		return false;
	}
	return true;
}

void UMallMultiplayerSubsystem::OnCreateComplete(const FName SessionName, const bool bSuccess)
{
	if (SessionName != NAME_GameSession || Operation != EOperation::Creating) return;
	const bool bShouldLeave = bLeaveAfterPendingOperation;
	bLeaveAfterPendingOperation = false;
	Finish(bSuccess, bSuccess ? TEXT("Session created. Loading the mall as listen server...") : TEXT("Session creation failed."));
	if (bShouldLeave) { LeaveSession(); return; }
	if (bSuccess)
	{
		HostedMaxPlayers = PendingMaxPlayers;
		// AGameSession::InitOptions reads MaxPlayers: the advertised cap also gates direct connections.
		UGameplayStatics::OpenLevel(this, FName(MallMultiplayer::TestMap), true, FString::Printf(TEXT("listen?MaxPlayers=%d"), HostedMaxPlayers));
	}
}

void UMallMultiplayerSubsystem::OnFindComplete(const bool bSuccess)
{
	if (Operation != EOperation::Finding) return;
	Results.Reset();
	JoinableResults.Reset();
	if (bSuccess && Search.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
		{
			FString Protocol;
			if (!Result.IsValid() || !Result.Session.SessionSettings.Get(MallMultiplayer::ProtocolKey, Protocol) || Protocol != MallMultiplayer::ProtocolValue) continue;
			FMallSessionListing& Listing = Results.AddDefaulted_GetRef();
			Listing.Index = JoinableResults.Add(Result);
			Listing.HostName = Result.Session.OwningUserName;
			Listing.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Listing.CurrentPlayers = FMath::Clamp(Listing.MaxPlayers - Result.Session.NumOpenPublicConnections, 0, Listing.MaxPlayers);
			Listing.PingMilliseconds = Result.PingInMs;
			UE_LOG(LogMallMultiplayer, Log, TEXT("[%d] %s (%d/%d players, %d ms)"), Listing.Index, *Listing.HostName, Listing.CurrentPlayers, Listing.MaxPlayers, Listing.PingMilliseconds);
		}
	}
	Finish(bSuccess, bSuccess ? FString::Printf(TEXT("Found %d mall session(s). Use mall.Join <index>."), Results.Num()) : TEXT("Session search failed."));
}

void UMallMultiplayerSubsystem::OnJoinComplete(const FName SessionName, const EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession || Operation != EOperation::Joining) return;
	const bool bShouldLeave = bLeaveAfterPendingOperation;
	bLeaveAfterPendingOperation = false;
	if (bShouldLeave)
	{
		Finish(Result == EOnJoinSessionCompleteResult::Success, TEXT("Pending join finished; leaving..."));
		LeaveSession();
		return;
	}
	FString Address;
	APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController();
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(NAME_GameSession, Address) || Address.IsEmpty() || !Controller)
	{
		Finish(false, FString::Printf(TEXT("Join failed: %s. Retry mall.Find if the host has changed."), LexToString(Result)));
		if (Sessions->GetNamedSession(NAME_GameSession)) Sessions->DestroySession(NAME_GameSession);
		return;
	}
	Finish(true, TEXT("Session joined. Connecting to the host..."));
	Controller->ClientTravel(Address, TRAVEL_Absolute);
}

void UMallMultiplayerSubsystem::OnDestroyComplete(const FName SessionName, const bool bSuccess)
{
	if (SessionName != NAME_GameSession || Operation != EOperation::Leaving) return;
	if (!bSuccess && Sessions.IsValid()) Sessions->RemoveNamedSession(NAME_GameSession);
	HostedMaxPlayers = 0;
	Finish(bSuccess, bSuccess ? TEXT("Session left; returning to local play.") : TEXT("Session cleanup failed; returning to local play."));
	ReturnToStandalone();
}

void UMallMultiplayerSubsystem::OnLoginComplete(int32 UserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error)
{
	if (UserNum != BoundLocalUserIndex || Operation != EOperation::LoggingIn) return;
	const ERequest Request = PendingRequest;
	Finish(bSuccess, bSuccess ? TEXT("Signed into EOS.") : TEXT("EOS login failed. Check your artifact/client policy; mall.Login explicitly opens Epic-account login."));
	if (!bSuccess) return;
	if (Request == ERequest::Host) HostSession(PendingMaxPlayers);
	else if (Request == ERequest::Find) FindSessions();
	else if (Request == ERequest::Join) JoinSession(PendingJoinIndex);
}

void UMallMultiplayerSubsystem::OnLoginStatusChanged(int32 UserNum, ELoginStatus::Type Previous, ELoginStatus::Type Current, const FUniqueNetId& UserId)
{
	if (UserNum == BoundLocalUserIndex && IsEOS() && Previous == ELoginStatus::LoggedIn && Current != ELoginStatus::LoggedIn)
	{
		CleanupFailedConnection(TEXT("EOS account signed out; multiplayer session closed."));
		ReturnToStandalone();
	}
}

void UMallMultiplayerSubsystem::SetStatus(const bool bSuccess, const FString& Message)
{
	Status = Message;
	UE_LOG(LogMallMultiplayer, Log, TEXT("%s"), *Message);
	OnStatusChanged.Broadcast(bSuccess, Message);
}

void UMallMultiplayerSubsystem::Finish(const bool bSuccess, const FString& Message)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(OperationTimer);
	ClearOperationDelegates();
	Operation = EOperation::Idle;
	bBusy = false;
	PendingRequest = ERequest::None;
	SetStatus(bSuccess, Message);
}

void UMallMultiplayerSubsystem::SetOperationTimeout()
{
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(OperationTimer, this, &UMallMultiplayerSubsystem::OperationTimedOut, 45.0f, false);
}

void UMallMultiplayerSubsystem::OperationTimedOut()
{
	const bool bWasLeaving = Operation == EOperation::Leaving || bLeaveAfterPendingOperation;
	if (Operation == EOperation::Finding && Sessions.IsValid()) Sessions->CancelFindSessions();
	bLeaveAfterPendingOperation = false;
	Finish(false, TEXT("Multiplayer request timed out. Check connectivity/account configuration and retry."));
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) && !Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->RemoveNamedSession(NAME_GameSession);
	}
	if (bWasLeaving) ReturnToStandalone();
}

void UMallMultiplayerSubsystem::ClearOperationDelegates()
{
	if (Sessions.IsValid())
	{
		if (CreateHandle.IsValid()) Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		if (FindHandle.IsValid()) Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		if (JoinHandle.IsValid()) Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		if (DestroyHandle.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	if (Identity.IsValid() && LoginHandle.IsValid()) Identity->ClearOnLoginCompleteDelegate_Handle(BoundLocalUserIndex, LoginHandle);
	CreateHandle.Reset(); FindHandle.Reset(); JoinHandle.Reset(); DestroyHandle.Reset(); LoginHandle.Reset();
}

void UMallMultiplayerSubsystem::ReleaseBackend()
{
	ClearOperationDelegates();
	if (Identity.IsValid() && LoginStatusHandle.IsValid()) Identity->ClearOnLoginStatusChangedDelegate_Handle(BoundLocalUserIndex, LoginStatusHandle);
	LoginStatusHandle.Reset();
	Identity.Reset();
	Sessions.Reset();
}

void UMallMultiplayerSubsystem::ReturnToStandalone()
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		UGameplayStatics::OpenLevel(this, FName(MallMultiplayer::TestMap), true);
	}
}

void UMallMultiplayerSubsystem::CleanupFailedConnection(const FString& Message)
{
	bLeaveAfterPendingOperation = false;
	if (Operation == EOperation::Finding && Sessions.IsValid()) Sessions->CancelFindSessions();
	Finish(false, Message);
	HostedMaxPlayers = 0;
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) && !Sessions->DestroySession(NAME_GameSession))
	{
		Sessions->RemoveNamedSession(NAME_GameSession);
	}
}

void UMallMultiplayerSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance()) CleanupFailedConnection(FString::Printf(TEXT("Network connection failed: %s"), *Error));
}

void UMallMultiplayerSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (World && World->GetGameInstance() == GetGameInstance()) CleanupFailedConnection(FString::Printf(TEXT("Map travel failed: %s"), *Error));
}
