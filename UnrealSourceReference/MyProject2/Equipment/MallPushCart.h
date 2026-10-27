#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallPushCart.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Bitflags for the 6 paranormal evidence types tracked in the Shared Cart Computer Logbook. */
namespace EMallGhostEvidenceBit
{
	constexpr uint8 None           = 0;
	constexpr uint8 EMFLevel5      = 1 << 0; // 0x01
	constexpr uint8 UVFingerprints = 1 << 1; // 0x02
	constexpr uint8 FreezingTemps  = 1 << 2; // 0x04
	constexpr uint8 SpiritBox      = 1 << 3; // 0x08
	constexpr uint8 GhostWriting   = 1 << 4; // 0x10
	constexpr uint8 MotionDetected = 1 << 5; // 0x20
}

/** Server-driven, swept cart with one pusher, storage for persistent item actors, and a Shared Team Computer Logbook. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallPushCart : public AActor
{
	GENERATED_BODY()
public:
	AMallPushCart();

	/** Configures a player character pawn for true First-Person perspective (eye-level camera, 0 boom length, 92 FOV, controller yaw). */
	UFUNCTION(BlueprintCallable, Category="Mall|FirstPerson")
	static bool ConfigurePawnFirstPersonView(APawn* Pawn, bool bEnableFirstPerson = true);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart")
	bool TryBeginPush(APlayerController* Player);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart")
	bool StopPushing(APlayerController* Player);
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	ACharacter* GetPusher() const { return Pusher; }
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	bool IsBeingPushed() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	bool IsParked() const { return IsSettled(); }
	/** Stable enough for storage, doorway protection and charging, with physics jitter hysteresis. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	bool IsSettled() const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Physics")
	void SetFreePhysicsEnabled(bool bEnabled);
	/** Shared forgiving focus query used by G/T/H and carried-tool F placement. */
	static AMallPushCart* FindCartInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart")
	bool TryDeposit(APlayerController* Player, AActor* Item);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart")
	bool TryRetrieve(APlayerController* Player, int32 SlotIndex = -1);
	/** Light carts override this to switch power or connect a nearby charging socket. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart")
	virtual bool TryUse(APlayerController* Player);
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	virtual FString GetCartStatus() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	bool CanPlayerReach(APlayerController* Player) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	bool CanPlayerPushFromHandle(const APlayerController* Player) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	int32 FindAimedStoredItemSlot(const FVector& Origin, const FVector& Direction) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	bool HasStoredItems() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart")
	static bool IsPawnPushingCart(const APawn* Pawn);
	static bool IsEligiblePlayer(const APlayerController* Player);
	bool ContainsStoredItem(const AActor* Item) const;
	FTransform GetSlotWorldTransform(int32 SlotIndex) const;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintImplementableEvent, Category="Mall|Cart|Events")
	void BP_OnPushStarted(ACharacter* NewPusher);
	UFUNCTION(BlueprintImplementableEvent, Category="Mall|Cart|Events")
	void BP_OnPushStopped(ACharacter* OldPusher);
	UFUNCTION(BlueprintImplementableEvent, Category="Mall|Cart|Events")
	void BP_OnItemStored(AActor* Item, int32 SlotIndex);
	UFUNCTION(BlueprintImplementableEvent, Category="Mall|Cart|Events")
	void BP_OnItemRetrieved(AActor* Item, ACharacter* Retriever);

	/** Centimetres/second. The normal test character moves at 600 cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Cart|Movement", meta=(ClampMin="25", ClampMax="600"))
	float PushSpeed = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Movement", meta=(ClampMin="30", ClampMax="180"))
	float TurnSpeedDegrees = 95.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Movement", meta=(ClampMin="40", ClampMax="120", Units="cm"))
	float HandleMinDistanceCm = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Movement", meta=(ClampMin="120", ClampMax="260", Units="cm"))
	float HandleMaxDistanceCm = 210.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Movement", meta=(ClampMin="50", ClampMax="160", Units="cm"))
	float HandleHalfWidthCm = 100.0f;
	/** Free carts are upright physics bodies; controlled G pushing temporarily uses swept movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Cart|Physics")
	bool bEnableFreePhysics = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Physics", meta=(ClampMin="20", ClampMax="500"))
	float CartMassKg = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Physics", meta=(ClampMin="0.1", ClampMax="20"))
	float FreeLinearDamping = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Physics", meta=(ClampMin="0.1", ClampMax="20"))
	float FreeAngularDamping = 5.0f;
	/** Limits ordinary CharacterMovement contact forces without changing the player's other physics interactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Physics", meta=(ClampMin="30", ClampMax="600"))
	float MaxBumpSpeedCmPerSecond = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Physics", meta=(ClampMin="10", ClampMax="180"))
	float MaxBumpAngularSpeedDegrees = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Physics", meta=(ClampMin="0.1", ClampMax="3"))
	float SettleDurationSeconds = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Storage", meta=(ClampMin="0", ClampMax="8"))
	int32 StorageCapacity = 8;
	UPROPERTY(ReplicatedUsing=OnRep_Storage, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Storage")
	TArray<TObjectPtr<AActor>> StoredItems;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Appearance")
	bool bShowProceduralCart = true;
	/** Replacement mesh: pivot at cart centre, 45 cm above its wheels' ground plane. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Appearance")
	TObjectPtr<UStaticMeshComponent> CartModel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart")
	FString CartName = TEXT("Equipment cart");

	// ========================================================================
	// Shared Team Computer Screen (Phasmophobia-Style Journal, Photo Log & Remote Cams)
	// ========================================================================

	/** True on the Equipment & Info-Screen Cart (AMallPushCart), false on the Light/Battery Safe-Zone Cart (AMallLightCart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Computer")
	bool bHasComputerTerminal = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UStaticMeshComponent> ComputerBaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UStaticMeshComponent> ComputerScreenMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UTextRenderComponent> ComputerHeaderText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UTextRenderComponent> ComputerBodyText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UTextRenderComponent> ComputerFooterText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UPointLightComponent> ComputerScreenLight;

	/** Dedicated view-target camera positioned in front of the cart monitor for full-screen terminal view. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<UCameraComponent> ComputerViewCamera;

	/** Single player currently operating the cart computer screen in full-screen view (exclusive to one player at a time). */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	TObjectPtr<ACharacter> ComputerOperator;

	/** Active tab on the shared cart computer screen: 0 = Evidence Journal, 1 = Saved Photos & Payout, 2 = Remote Cameras & Sensors. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	uint8 ActiveComputerTab = 0;

	/** Combined bitmask of auto-harvested and manually logged paranormal evidence (EMallGhostEvidenceBit). */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	uint8 SharedEvidenceMask = 0;

	/** Bitmask of evidence automatically confirmed by team equipment (EMF-5, UV, Freezing, Spirit Box, Ghost Writing, Motion). */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	uint8 AutoDetectedEvidenceMask = 0;

	/** Bitmask of evidence manually logged/toggled by players at the cart computer terminal. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	uint8 ManualEvidenceMask = 0;

	/** Deduced ghost archetype or candidate list computed from SharedEvidenceMask. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	FString DeducedGhostArchetype = TEXT("UNDETERMINED (0/3 Evidence)");

	/** Total photos captured across all team AMallPhotoCamera devices. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	int32 SyncedPhotoCount = 0;

	/** Highest star rating (1..5) recorded across team photos. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	int32 SyncedBestPhotoStars = 0;

	/** Cumulative cash reward ($) from all captured team photos. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	int32 SyncedTotalPhotoReward = 0;

	/** Subject label of the most recent paranormal photo. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	FString SyncedLastPhotoSubject = TEXT("No Photos Logged");

	/** Currently selected remote camera/sensor feed index on Tab 2. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	int32 SelectedRemoteFeedIndex = 0;

	/** Live telemetry readout of the active remote camera/sensor feed on Tab 2. */
	UPROPERTY(ReplicatedUsing=OnRep_ComputerState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Computer")
	FString SyncedRemoteFeedStatus = TEXT("No Remote Cameras/Sensors Deployed");

	/** Synchronizes shared evidence, saved photos, and remote camera/sensor telemetry from world and stored equipment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	void SyncComputerFromWorldEquipment();

	/** Returns the player character currently operating this cart's computer in full-screen mode, if any. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Computer")
	ACharacter* GetComputerOperator() const { return ComputerOperator; }

	/** Returns true if a player is currently logged into the cart computer screen. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Computer")
	bool IsComputerInUse() const;

	/** Returns true if the given pawn is currently logged into this cart's computer screen. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Computer")
	bool IsComputerInUseBy(const APawn* Pawn) const;

	/** Enters exclusive full-screen computer view for Player if no other player is currently on the screen. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	bool TryBeginUseComputer(APlayerController* Player);

	/** Releases the exclusive computer terminal lock held by Player. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	bool StopUseComputer(APlayerController* Player);

	/** Toggles full-screen computer terminal view for Player. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	bool TryToggleUseComputer(APlayerController* Player);

	/** Cycles the shared cart computer screen tab (0: Evidence Journal -> 1: Saved Photos -> 2: Remote Cams/Sensors). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	bool TryCycleComputerTab(APlayerController* Player);

	/** Toggles a manual evidence bit on the shared cart computer journal. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	bool TryToggleEvidenceBit(APlayerController* Player, uint8 EvidenceBit);

	/** Cycles through deployed remote cameras/sensors on Tab 2, or remotely triggers the selected placed Photo Camera! */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Computer")
	bool TryCycleRemoteFeed(APlayerController* Player, bool bTriggerRemoteShutter = false);

	/** Evaluates the ghost archetype or narrowed candidate list from a 6-bit evidence mask. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Computer")
	static FString DeduceGhostTypeFromEvidence(uint8 EvidenceMask);

	/** Returns a one-line status summary of the shared Cart Computer Screen for HUD prompts. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Computer")
	FString GetComputerSummaryText() const;

	/** Refreshes the 3D text and screen glow on the cart computer terminal. */
	UFUNCTION(BlueprintCallable, Category="Mall|Cart|Computer")
	void UpdateComputerVisuals();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void OnPushStateChanged();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart")
	TObjectPtr<UBoxComponent> CartBounds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart")
	TObjectPtr<USceneComponent> CartVisualRoot;
	UPROPERTY(ReplicatedUsing=OnRep_Pusher, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart")
	TObjectPtr<ACharacter> Pusher;

private:
	UFUNCTION() void OnRep_Pusher();
	UFUNCTION() void OnRep_Settled();
	UFUNCTION() void OnRep_ComputerState();
	void RefreshPhysicsMode();
	void UpdateFreePhysics(float DeltaSeconds);
	void SetSettled(bool bNewSettled);
	UFUNCTION() void OnRep_Storage();
	void ApplyPusherSettings();
	void RestorePusherSettings();
	void EndPush();
	bool HasSafeFloor(const FVector& Centre, const FQuat& Rotation) const;
	void FollowPusher(float DeltaSeconds);
	void ReconcileStorage();
	TWeakObjectPtr<ACharacter> ConfiguredPusher;
	TWeakObjectPtr<APlayerController> PusherController;
	TWeakObjectPtr<APlayerController> ComputerOperatorController;
	float SavedWalkSpeed = 0.0f;
	float SavedCrouchedSpeed = 0.0f;
	float AppliedWalkSpeed = 0.0f;
	float AppliedCrouchedSpeed = 0.0f;
	bool bSavedControllerYaw = false;
	bool bSavedOrientToMovement = false;
	bool bSavedControllerDesiredRotation = false;
	float PushDistance = 110.0f;
	UPROPERTY(ReplicatedUsing=OnRep_Settled, VisibleAnywhere, Category="Mall|Cart|Physics")
	bool bSettled = true;
	float StablePhysicsSeconds = 0.0f;
	FVector SettledLocation = FVector::ZeroVector;
	float SettledYaw = 0.0f;
	bool bPhysicsConfigured = false;
	float AppliedMassKg = -1.0f;
	float AppliedLinearDamping = -1.0f;
	float AppliedAngularDamping = -1.0f;
	float ComputerSyncAccumulator = 0.0f;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ComputerScreenMID;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ProceduralParts;
};
