#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallGhostDisturbanceProp.generated.h"

class UStaticMeshComponent;

/** Explicitly opted-in, harmless physics prop; generic scene items cannot be thrown. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallGhostDisturbanceProp : public AActor
{
	GENERATED_BODY()

public:
	AMallGhostDisturbanceProp();

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Props")
	bool CanBeDisturbedBy(AActor* Ghost) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Props")
	bool Disturb(AActor* Ghost, bool bThrow);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Props")
	TObjectPtr<UStaticMeshComponent> PropMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Props", meta=(ClampMin="20.0", ClampMax="300.0"))
	float MaximumSpeedCmPerSecond = 250.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Props")
	int32 DisturbanceCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Props")
	bool bLastDisturbanceWasThrow = false;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Props")
	int32 BoundaryRecoveryCount = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void EnforceHarmlessPhysics();
	void RecoverToHome();
	TWeakObjectPtr<AActor> DisturbingGhost;
	FTransform SafeHomeTransform;
	bool bHasSafeHome = false;
	double NextDisturbanceTime = 0.0;
};
