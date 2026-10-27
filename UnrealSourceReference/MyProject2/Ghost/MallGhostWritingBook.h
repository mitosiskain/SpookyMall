#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Equipment/MallCartItem.h"
#include "MallGhostWritingBook.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class APawn;
class APlayerController;
class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;

/** An open evidence book. The server replicates writing state rather than texture pixels. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallGhostWritingBook : public AActor, public IMallCartItem
{
	GENERATED_BODY()

public:
	AMallGhostWritingBook();

	virtual APawn* GetCartItemCarrier_Implementation() const override { return GetCarrier(); }
	virtual AMallPushCart* GetCartStorage_Implementation() const override { return StoredOnCart; }
	virtual bool TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform) override;
	virtual bool TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart) override;
	virtual void ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform) override;
	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart")
	TObjectPtr<AMallPushCart> StoredOnCart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Book")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Book")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Book")
	int32 CarriedHotbarSlot = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Book")
	int32 CarrierSelectedSlot = 0;

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Book")
	bool IsSelectedByCarrier() const;

	void SetCarrierSelectedSlot(int32 NewSelectedSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Book")
	bool TryPickup(APlayerController* Player);

	/** Accepts only a server-validated, supported, flat placement near the current carrier. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Book")
	bool TryPlace(APlayerController* Player, const FTransform& Placement);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Book")
	bool TrySetOpen(APlayerController* Player, bool bNewOpen);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Book")
	APawn* GetCarrier() const;

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Book")
	bool IsPlacedForWriting() const;

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Book")
	bool IsOpen() const { return bOpen; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Book")
	bool TryWrite(AActor* Ghost);

	/** True once writing has started; subsequent attempts leave the same evidence intact. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Book")
	bool IsWritten() const { return bWritten; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Book")
	void ResetWriting();

	/** Optional imported open-book model. The page overlay remains available with this model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Appearance, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMesh> BookMesh;

	/** Material parameters: WritingTexture (texture) and WritingProgress (scalar, 0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Appearance, Category="Mall|Ghost|Book")
	TObjectPtr<UMaterialInterface> PageMaterial;

	/** Replace this asset with transparent handwriting artwork; no render target is required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Appearance, Category="Mall|Ghost|Book")
	TObjectPtr<UTexture2D> WritingTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Appearance, Category="Mall|Ghost|Book", meta=(ClampMin="0.1", ClampMax="30.0"))
	float WritingDuration = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Book")
	float WritingProgress = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Appearance, Category="Mall|Ghost|Book")
	FString TestWriting = TEXT("GET\nOUT");

	UPROPERTY(ReplicatedUsing=OnRep_Writing, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Book")
	bool bWritten = false;

	UPROPERTY(ReplicatedUsing=OnRep_Writing, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Book")
	double WritingStartedAtServerTime = -1.0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_Writing();
	UFUNCTION()
	void OnRep_Appearance();
	UFUNCTION()
	void OnRep_InteractionState();
	UFUNCTION()
	void OnCarrierDestroyed(AActor* DestroyedActor);
	void RefreshVisuals();
	void ApplyInteractionPresentation();
	void SyncHotbarState();
	void UpdateWritingProgress();
	void FollowCarrier();
	void RestoreLastPlacement();
	void ClearCarrier();
	bool IsPlayerEligible(APlayerController* Player) const;
	bool CanReachPlacedBook(APlayerController* Player) const;
	bool ValidatePlacement(APlayerController* Player, const FTransform& Requested, FTransform& Supported) const;
	double GetSynchronizedTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<APawn> Carrier;
	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, Category="Mall|Ghost|Book")
	bool bOpen = false;
	TWeakObjectPtr<APlayerController> CarrierController;
	TWeakObjectPtr<APawn> LastSyncedCarrier;
	int32 LastSyncedSlot = INDEX_NONE;
	bool bLastSelectedByCarrier = true;
	FTransform LastPlacedTransform = FTransform::Identity;
	bool bHasLastPlacedTransform = false;

	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<USceneComponent> BookRoot;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UBoxComponent> InteractionBounds;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMeshComponent> BookModel;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMeshComponent> LeftCover;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMeshComponent> RightCover;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMeshComponent> LeftPages;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMeshComponent> RightPages;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UStaticMeshComponent> WritingPage;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Book")
	TObjectPtr<UTextRenderComponent> FallbackWriting;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PageMID;
};
