#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Subsystems/WorldSubsystem.h"
#include "MallBookInteractionComponent.generated.h"

class AMallGhostWritingBook;
class AMallHandheldItem;
class AMallSpiritBox;
class APlayerController;
struct FCollisionQueryParams;
class UInputComponent;
class UMallBookInteractionWidget;

/** Owner RPC endpoint for handheld evidence tools; target and placement are derived on the server. */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallBookInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMallBookInteractionComponent();
	bool CanAcceptCartItem(const AActor* Item = nullptr);
	bool AdoptCartItem(AActor* Item);
	UFUNCTION(BlueprintCallable, Category="Mall|Book") void RequestInteract();
	UFUNCTION(BlueprintCallable, Category="Mall|Book") void RequestToggleOpen();
	UFUNCTION(BlueprintCallable, Category="Mall|Book") void RequestSecondaryInteract();
	UFUNCTION(BlueprintCallable, Category="Mall|Book") bool RequestDrop();
	UFUNCTION(BlueprintPure, Category="Mall|Book") bool HasHeldEvidenceItem() const;
	UFUNCTION(BlueprintPure, Category="Mall|Book") bool HasFocusedEvidenceItem() const;
	UFUNCTION(BlueprintPure, Category="Mall|Book") AMallGhostWritingBook* GetHeldBook() const { return HeldBook; }
	UFUNCTION(BlueprintPure, Category="Mall|Book") AMallGhostWritingBook* GetActiveHeldBook() const;
	UFUNCTION(BlueprintPure, Category="Mall|Book") AMallGhostWritingBook* GetFocusedBook() const;
	UFUNCTION(BlueprintPure, Category="Mall|SpiritBox") AMallSpiritBox* GetHeldSpiritBox() const { return HeldSpiritBox; }
	UFUNCTION(BlueprintPure, Category="Mall|SpiritBox") AMallSpiritBox* GetActiveHeldSpiritBox() const;
	UFUNCTION(BlueprintPure, Category="Mall|SpiritBox") AMallSpiritBox* GetFocusedSpiritBox() const;
	UFUNCTION(BlueprintPure, Category="Mall|Equipment") AMallHandheldItem* GetActiveHeldEquipment() const;
	UFUNCTION(BlueprintPure, Category="Mall|Equipment") AMallHandheldItem* GetFocusedEquipment() const;
	UFUNCTION(BlueprintPure, Category="Mall|Equipment") const TArray<AMallHandheldItem*>& GetHeldEquipmentList() const { return HeldEquipment; }
	UPROPERTY(BlueprintReadOnly, Transient, Category="Mall|Book") FString LastFeedback;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FMallBookInteractionTest;
	friend class FMallSpiritBoxTest;
	friend class FMallSpiritBoxNetworkTest;
	friend class FMallGhostEquipmentTest;
#endif
	UFUNCTION(Server, Reliable) void ServerInteract(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);
	UFUNCTION(Server, Reliable) void ServerToggleOpen();
	UFUNCTION(Server, Reliable) void ServerToggleSpiritBox(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);
	UFUNCTION(Server, Reliable) void ServerCycleSpiritBoxChannel(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);
	UFUNCTION(Server, Reliable) void ServerPrimaryUseEquipment(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);
	UFUNCTION(Server, Reliable) void ServerSecondaryUseEquipment(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection);
	UFUNCTION(Server, Reliable) void ServerDropHeldItem();
	UFUNCTION(Client, Reliable) void ClientFeedback(const FString& Message);
	APlayerController* GetController() const;
	bool IsValidView(const FVector& Origin, const FVector& Direction) const;
	void AddIgnoredHeldActors(FCollisionQueryParams& Params) const;
	AMallGhostWritingBook* FindBook(const FVector& Origin, const FVector& Direction) const;
	AMallSpiritBox* FindSpiritBox(const FVector& Origin, const FVector& Direction) const;
	AMallHandheldItem* FindEquipment(const FVector& Origin, const FVector& Direction) const;
	bool BuildPlacement(const FVector& Origin, const FVector& Direction, FTransform& Result, FString& Failure) const;
	bool BuildSpiritBoxPlacement(const FVector& Origin, const FVector& Direction, FTransform& Result, FString& Failure) const;
	bool BuildEquipmentPlacement(const AMallHandheldItem* Item, const FVector& Origin, const FVector& Direction, FTransform& Result, FString& Failure) const;
	bool HasPawnLineOfSight(const FVector& Target, const AActor* AllowedHit) const;
	void ClearStaleHeldItems();
	void Feedback(const FString& Message);
	UPROPERTY(Replicated) TObjectPtr<AMallGhostWritingBook> HeldBook;
	UPROPERTY(Replicated) TObjectPtr<AMallSpiritBox> HeldSpiritBox;
	UPROPERTY(Replicated) TArray<TObjectPtr<AMallHandheldItem>> HeldEquipment;
	UPROPERTY(Transient) TObjectPtr<UMallBookInteractionWidget> Widget;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	bool bInteractDown = false;
	bool bOpenDown = false;
	double NextActionTime = 0.0;
	double FeedbackExpiresAt = 0.0;
};

/** Attach endpoints independently of voice settings and without reparenting the player's Blueprint. */
UCLASS()
class MYPROJECT2_API UMallBookInteractionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
private:
	void AttachEndpoints();
	double NextRefreshTime = 0.0;
};
