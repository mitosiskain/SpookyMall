#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace MallHotbarInventory
{
	inline UActorComponent* FindInventoryComponent(const APawn* Pawn)
	{
		if (!IsValid(Pawn)) { return nullptr; }
		TInlineComponentArray<UActorComponent*> Components;
		Pawn->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component && Component->GetClass()->GetName() == TEXT("BP_InventoryComponent_C"))
			{
				return Component;
			}
		}
		return nullptr;
	}

	inline bool HasInventory(const APawn* Pawn)
	{
		return FindInventoryComponent(Pawn) != nullptr;
	}

	inline FProperty* FindStructMember(UStruct* Struct, const TCHAR* Stem)
	{
		if (!Struct) { return nullptr; }
		const FString Prefix = FString(Stem) + TEXT("_");
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName() == Stem || It->GetName().StartsWith(Prefix))
			{
				return *It;
			}
		}
		return nullptr;
	}

	struct FSlotsAccess
	{
		UActorComponent* Component = nullptr;
		FArrayProperty* Array = nullptr;
		FStructProperty* Item = nullptr;
		FTextProperty* DisplayName = nullptr;
		FObjectPropertyBase* Icon = nullptr;
		FNumericProperty* ItemValue = nullptr;
		FByteProperty* Type = nullptr;
		FIntProperty* Selected = nullptr;

		explicit FSlotsAccess(const APawn* Pawn)
		{
			Component = FindInventoryComponent(Pawn);
			if (!Component) { return; }
			Array = FindFProperty<FArrayProperty>(Component->GetClass(), TEXT("Slots"));
			Selected = FindFProperty<FIntProperty>(Component->GetClass(), TEXT("SelectedIndex"));
			Item = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
			DisplayName = Item ? CastField<FTextProperty>(FindStructMember(Item->Struct, TEXT("DisplayName"))) : nullptr;
			Icon = Item ? CastField<FObjectPropertyBase>(FindStructMember(Item->Struct, TEXT("Icon"))) : nullptr;
			ItemValue = Item ? CastField<FNumericProperty>(FindStructMember(Item->Struct, TEXT("ItemValue"))) : nullptr;
			Type = Item ? CastField<FByteProperty>(FindStructMember(Item->Struct, TEXT("ItemType"))) : nullptr;
		}

		bool Valid() const
		{
			return Component && Array && Item && DisplayName && Icon && Type && Selected && Type->Enum;
		}

		uint8 FlashlightType() const
		{
			if (!Valid()) { return 1; }
			for (int32 Index = 0; Index < Type->Enum->NumEnums(); ++Index)
			{
				if (Type->Enum->GetDisplayNameTextByIndex(Index).ToString() == TEXT("Flashlight"))
				{
					return static_cast<uint8>(Type->Enum->GetValueByIndex(Index));
				}
			}
			return 1;
		}

		uint8 GenericType() const
		{
			if (!Valid()) { return 0; }
			const uint8 Flash = FlashlightType();
			for (int32 Index = 0; Index < Type->Enum->NumEnums(); ++Index)
			{
				const uint8 Val = static_cast<uint8>(Type->Enum->GetValueByIndex(Index));
				if (Val != Flash) { return Val; }
			}
			return 0;
		}

		bool IsOccupied(const void* Data) const
		{
			return Valid() && Data && Icon->GetObjectPropertyValue_InContainer(Data) != nullptr;
		}

		bool MatchesName(const void* Data, const FText& ExpectedName) const
		{
			return IsOccupied(Data) && DisplayName->GetPropertyValue_InContainer(Data).EqualTo(ExpectedName);
		}
	};

	inline void Notify(APawn* Pawn, UActorComponent* Component)
	{
		if (Component)
		{
			if (FMulticastDelegateProperty* Changed = FindFProperty<FMulticastDelegateProperty>(Component->GetClass(), TEXT("OnInventoryChanged")))
			{
				if (FMulticastScriptDelegate* Delegate = Changed->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Component))
				{
					Delegate->ProcessDelegate<UObject>(nullptr);
				}
			}
			if (FMulticastDelegateProperty* SelChanged = FindFProperty<FMulticastDelegateProperty>(Component->GetClass(), TEXT("OnSelectionChanged")))
			{
				if (FMulticastScriptDelegate* Delegate = SelChanged->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Component))
				{
					const FIntProperty* SelectedProp = FindFProperty<FIntProperty>(Component->GetClass(), TEXT("SelectedIndex"));
					struct { int32 NewIndex; } Params{ SelectedProp ? SelectedProp->GetPropertyValue_InContainer(Component) : 0 };
					Delegate->ProcessDelegate<UObject>(&Params);
				}
			}
		}
		if (Pawn)
		{
			if (UFunction* Refresh = Pawn->FindFunction(TEXT("RefreshFlashlight")))
			{
				Pawn->ProcessEvent(Refresh, nullptr);
			}
		}
	}

	inline int32 GetSelectedSlot(const APawn* Pawn)
	{
		FSlotsAccess Access(Pawn);
		return Access.Valid() ? Access.Selected->GetPropertyValue_InContainer(Access.Component) : INDEX_NONE;
	}

	inline int32 FindItemSlot(const APawn* Pawn, const FText& ExpectedName, int32 PreferredSlot = INDEX_NONE)
	{
		FSlotsAccess Access(Pawn);
		if (!Access.Valid()) { return INDEX_NONE; }
		FScriptArrayHelper Slots(Access.Array, Access.Array->ContainerPtrToValuePtr<void>(Access.Component));
		if (Slots.IsValidIndex(PreferredSlot) && Access.MatchesName(Slots.GetRawPtr(PreferredSlot), ExpectedName))
		{
			return PreferredSlot;
		}
		for (int32 I = 0; I < Slots.Num(); ++I)
		{
			if (Access.MatchesName(Slots.GetRawPtr(I), ExpectedName))
			{
				return I;
			}
		}
		return INDEX_NONE;
	}

	inline int32 FindFreeSlot(const APawn* Pawn, const FText& ReuseDisplayName = FText::GetEmpty())
	{
		FSlotsAccess Access(Pawn);
		if (!Access.Valid()) { return INDEX_NONE; }
		if (!ReuseDisplayName.IsEmpty())
		{
			const int32 Existing = FindItemSlot(Pawn, ReuseDisplayName);
			if (Existing != INDEX_NONE) { return Existing; }
		}
		FScriptArrayHelper Slots(Access.Array, Access.Array->ContainerPtrToValuePtr<void>(Access.Component));
		const int32 SelectedIdx = Access.Selected->GetPropertyValue_InContainer(Access.Component);
		if (Slots.IsValidIndex(SelectedIdx) && !Access.IsOccupied(Slots.GetRawPtr(SelectedIdx)))
		{
			return SelectedIdx;
		}
		for (int32 I = 0; I < Slots.Num(); ++I)
		{
			if (!Access.IsOccupied(Slots.GetRawPtr(I)))
			{
				return I;
			}
		}
		return INDEX_NONE;
	}

	inline bool CanAcceptItem(const APawn* Pawn, const FText& DisplayName)
	{
		if (!IsValid(Pawn)) { return false; }
		if (!HasInventory(Pawn)) { return true; }
		return FindFreeSlot(Pawn, DisplayName) != INDEX_NONE;
	}

	inline int32 EnsureItemInSlot(
		APawn* Pawn,
		const FText& ItemName,
		UTexture2D* ItemIcon,
		double Value,
		bool bIsFlashlight,
		int32 PreferredSlot,
		bool bSelectSlot)
	{
		if (!IsValid(Pawn)) { return INDEX_NONE; }
		FSlotsAccess Access(Pawn);
		if (!Access.Valid())
		{
			return PreferredSlot != INDEX_NONE ? PreferredSlot : 0;
		}
		if (!ItemIcon) { return INDEX_NONE; }

		FScriptArrayHelper Slots(Access.Array, Access.Array->ContainerPtrToValuePtr<void>(Access.Component));
		int32 TargetSlot = FindItemSlot(Pawn, ItemName, PreferredSlot);
		if (TargetSlot == INDEX_NONE)
		{
			if (Slots.IsValidIndex(PreferredSlot) && !Access.IsOccupied(Slots.GetRawPtr(PreferredSlot)))
			{
				TargetSlot = PreferredSlot;
			}
			else
			{
				TargetSlot = FindFreeSlot(Pawn, ItemName);
			}
		}
		if (!Slots.IsValidIndex(TargetSlot)) { return INDEX_NONE; }

		void* Data = Slots.GetRawPtr(TargetSlot);
		const uint8 DesiredType = bIsFlashlight ? Access.FlashlightType() : Access.GenericType();
		const bool bSlotMatches =
			Access.DisplayName->GetPropertyValue_InContainer(Data).EqualTo(ItemName) &&
			Access.Icon->GetObjectPropertyValue_InContainer(Data) == ItemIcon &&
			Access.Type->GetPropertyValue_InContainer(Data) == DesiredType;

		bool bChanged = false;
		if (!bSlotMatches)
		{
			Access.DisplayName->SetPropertyValue_InContainer(Data, ItemName);
			Access.Icon->SetObjectPropertyValue_InContainer(Data, ItemIcon);
			if (Access.ItemValue && Access.ItemValue->IsFloatingPoint())
			{
				Access.ItemValue->SetFloatingPointPropertyValue(Access.ItemValue->ContainerPtrToValuePtr<void>(Data), Value);
			}
			Access.Type->SetPropertyValue_InContainer(Data, DesiredType);
			bChanged = true;
		}

		if (bSelectSlot && Access.Selected->GetPropertyValue_InContainer(Access.Component) != TargetSlot)
		{
			Access.Selected->SetPropertyValue_InContainer(Access.Component, TargetSlot);
			bChanged = true;
		}

		if (bChanged)
		{
			Notify(Pawn, Access.Component);
		}
		return TargetSlot;
	}

	inline bool RemoveItem(APawn* Pawn, const FText& ExpectedName, int32 PreferredSlot = INDEX_NONE)
	{
		if (!IsValid(Pawn)) { return false; }
		FSlotsAccess Access(Pawn);
		if (!Access.Valid()) { return false; }
		FScriptArrayHelper Slots(Access.Array, Access.Array->ContainerPtrToValuePtr<void>(Access.Component));
		bool bChanged = false;
		auto ClearSlot = [&](int32 Index)
		{
			if (!Slots.IsValidIndex(Index)) { return; }
			void* Data = Slots.GetRawPtr(Index);
			if (Access.MatchesName(Data, ExpectedName))
			{
				Access.Item->Struct->DestroyStruct(Data);
				Access.Item->Struct->InitializeStruct(Data);
				bChanged = true;
			}
		};
		if (PreferredSlot != INDEX_NONE)
		{
			ClearSlot(PreferredSlot);
		}
		for (int32 I = 0; I < Slots.Num(); ++I)
		{
			ClearSlot(I);
		}
		if (bChanged)
		{
			Notify(Pawn, Access.Component);
		}
		return bChanged;
	}
}
