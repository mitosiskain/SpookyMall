#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MallCartItem.generated.h"

class AMallPushCart;
class APawn;
class APlayerController;

/** Actual item actors move into storage; evidence and per-item state are never cloned. */
UINTERFACE(BlueprintType)
class MYPROJECT2_API UMallCartItem : public UInterface
{
	GENERATED_BODY()
};

class MYPROJECT2_API IMallCartItem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mall|Cart|Item")
	APawn* GetCartItemCarrier() const;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mall|Cart|Item")
	AMallPushCart* GetCartStorage() const;
	/** Implementations validate their own carrier, clear it, then enter inactive stored state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mall|Cart|Item")
	bool TryStoreOnCart(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform);
	/** Implementations restore normal pickup and the player's appropriate interaction endpoint. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mall|Cart|Item")
	bool TryTakeFromCart(APlayerController* Player, AMallPushCart* Cart);
	/** Server-only cleanup when a cart disappears; retain the item's existing evidence. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mall|Cart|Item")
	void ReleaseFromCart(AMallPushCart* Cart, const FTransform& WorldTransform);
};
