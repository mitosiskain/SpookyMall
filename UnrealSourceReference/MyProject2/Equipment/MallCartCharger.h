#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallCartCharger.generated.h"

class AMallLightCart;
class APlayerController;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Place at a wall socket or at the powered end of an extension lead. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallCartCharger : public AActor
{
	GENERATED_BODY()

public:
	AMallCartCharger();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Charging")
	bool TryUse(APlayerController* Player);
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Charging")
	bool CanSupply(const AMallLightCart* Cart) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Charging")
	FVector GetConnectionLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Charging")
	bool bPowered = true;
	/** Maximum unobstructed cable reach; increase this for a placed extension lead endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Charging", meta=(ClampMin="50.0", ClampMax="3000.0", Units="cm"))
	float ConnectionRangeCm = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Charging")
	bool bExtensionLead = false;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Charging")
	TObjectPtr<UBoxComponent> ChargerBounds;

	virtual void OnConstruction(const FTransform& Transform) override;
private:
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Charging")
	TObjectPtr<UStaticMeshComponent> OutletMesh;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Charging")
	TObjectPtr<UTextRenderComponent> ChargerDisplay;
};
