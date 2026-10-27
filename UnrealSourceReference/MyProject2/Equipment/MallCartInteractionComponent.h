#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Subsystems/WorldSubsystem.h"
#include "MallCartInteractionComponent.generated.h"

class AMallPushCart;
class AMallCartCharger;
class AMallCartFlashlight;
class APawn;
class APlayerController;
class UInputComponent;
class UMallCartInteractionWidget;

/** Owner-only RPC entrypoint; the server derives and validates the cart from the player's view. */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallCartInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMallCartInteractionComponent();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestPrimaryInteract();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestSecondaryInteract();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") bool RequestDropOrRelease();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") bool RequestToggleFlashlight();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestPush();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestTransfer();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestUse();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestRetrieveSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestCycleComputerTab();
	UFUNCTION(BlueprintCallable, Category="Mall|Cart") void RequestComputerRemoteFeed();
	UFUNCTION(BlueprintPure, Category="Mall|Cart") AMallPushCart* GetFocusedCart() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart") AMallCartCharger* GetFocusedCharger() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart") AMallCartFlashlight* GetFocusedFlashlight() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart") AMallPushCart* GetActiveCart() const { return ActiveCart; }
	UFUNCTION(BlueprintPure, Category="Mall|Cart") AMallPushCart* GetLocalActiveComputerCart() const;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Mall|Cart") FString LastFeedback;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FMallCartMovementStorageNetworkTest;
	friend class FMallCartFlashlightNetworkTest;
#endif
	UFUNCTION(Server, Reliable)
	void ServerCartAction(uint8 Action, FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, int32 SlotIndex);
	UFUNCTION(Server, Unreliable) void ServerFlashlightState(int32 SelectedSlot, bool bLightOn);
	UFUNCTION(Client, Reliable) void ClientFeedback(const FString& Message);
	APlayerController* GetController() const;
	bool IsValidView(const FVector& Origin, const FVector& Direction) const;
	AMallPushCart* FindCart(const FVector& Origin, const FVector& Direction) const;
	AMallCartCharger* FindCharger(const FVector& Origin, const FVector& Direction) const;
	AMallCartFlashlight* FindFlashlight(const FVector& Origin, const FVector& Direction) const;
	AActor* FindCarriedItem() const;
	void RequestAction(uint8 Action, int32 SlotIndex = -1);
	void OnKeyDropOrRelease();
	void OnKeyToggleFlashlight();
	UPROPERTY(Replicated) TObjectPtr<AMallPushCart> ActiveCart;
	UPROPERTY(Transient) TObjectPtr<UMallCartInteractionWidget> Widget;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	TWeakObjectPtr<APawn> LastSyncedPawn;
	TWeakObjectPtr<APawn> LastConfiguredFirstPersonPawn;
	bool bLastConfiguredFirstPersonMode = true;
	TWeakObjectPtr<AMallPushCart> LocalViewedComputerCart;
	int32 LastSyncedSelectedSlot = INDEX_NONE;
	bool bLastSyncedLightOn = false;
	double NextActionTime = 0;
	double NextLocalInputTime = 0;
	double FeedbackUntil = 0;
	double NextFlashlightSync = 0;
	double NextServerFlashlightSync = 0;
};

/** Adds the owner RPC endpoint to new and late-joining controllers without changing the player Blueprint. */
UCLASS()
class MYPROJECT2_API UMallCartInteractionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
private:
	void AttachEndpoints();
	double NextRefresh = 0;
};
