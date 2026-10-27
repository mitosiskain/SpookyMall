#include "Ghost/MallGhostStateTree.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Engine/World.h"
#include "Environment/MallSpaAndFoodSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ghost/MallGhostCartHelpers.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeReference.h"
#include "StateTreeState.h"
#include "UObject/SavePackage.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameMapsSettings.h"
#include "Ghost/MallGhostSoundComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/NetTestHelpers.h"
#include "Voice/MallVoiceSettings.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMallGhostStateTree, Log, All);

namespace MallGhostAI
{
	double ReadDouble(const UObject* Object, const FName Name, const double Default = 0.0)
	{
		const FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint()) { return Default; }
		const double Value = Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object));
		return FMath::IsFinite(Value) ? Value : Default;
	}

	void WriteDouble(UObject* Object, const FName Name, const double Value)
	{
		FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (Property && Property->IsFloatingPoint() && FMath::IsFinite(Value))
		{
			Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
		}
	}

	int32 ReadInt(const UObject* Object, const FName Name, const int32 Default = 0)
	{
		const FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsInteger()) { return Default; }
		return static_cast<int32>(Property->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object)));
	}

	void WriteInt(UObject* Object, const FName Name, const int32 Value)
	{
		FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (Property && Property->IsInteger())
		{
			Property->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), static_cast<int64>(Value));
		}
	}

	bool ReadBool(const UObject* Object, const FName Name, const bool Default = false)
	{
		const FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : Default;
	}

	void WriteBool(UObject* Object, const FName Name, const bool Value)
	{
		if (FBoolProperty* Property = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr)
		{
			Property->SetPropertyValue_InContainer(Object, Value);
		}
	}

	FString ReadString(const UObject* Object, const FName Name)
	{
		const FStrProperty* Property = Object ? FindFProperty<FStrProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : FString();
	}

	void WriteString(UObject* Object, const FName Name, const FString& Value)
	{
		if (FStrProperty* Property = Object ? FindFProperty<FStrProperty>(Object->GetClass(), Name) : nullptr)
		{
			Property->SetPropertyValue_InContainer(Object, Value);
		}
	}

	FVector ReadVector(const UObject* Object, const FName Name, const FVector Default = FVector::ZeroVector)
	{
		const FStructProperty* Property = Object ? FindFProperty<FStructProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || Property->Struct != TBaseStructure<FVector>::Get()) { return Default; }
		const FVector Value = *Property->ContainerPtrToValuePtr<FVector>(Object);
		return Value.ContainsNaN() ? Default : Value;
	}

	void WriteVector(UObject* Object, const FName Name, const FVector& Value)
	{
		FStructProperty* Property = Object ? FindFProperty<FStructProperty>(Object->GetClass(), Name) : nullptr;
		if (Property && Property->Struct == TBaseStructure<FVector>::Get() && !Value.ContainsNaN())
		{
			*Property->ContainerPtrToValuePtr<FVector>(Object) = Value;
		}
	}

	TArray<FVector>* GetVectorArrayPtr(UObject* Object, const FName Name)
	{
		FArrayProperty* ArrayProp = Object ? FindFProperty<FArrayProperty>(Object->GetClass(), Name) : nullptr;
		const FStructProperty* InnerProp = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
		if (!InnerProp || InnerProp->Struct != TBaseStructure<FVector>::Get()) { return nullptr; }
		return ArrayProp->ContainerPtrToValuePtr<TArray<FVector>>(Object);
	}

	template <typename T>
	T* ReadObject(const UObject* Object, const FName Name)
	{
		const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}

	void WriteObject(UObject* Object, const FName Name, UObject* Value)
	{
		FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		if (Property && (!Value || Value->IsA(Property->PropertyClass)))
		{
			Property->SetObjectPropertyValue_InContainer(Object, Value);
		}
	}

	ACharacter* ResolveGhostCharacter(FStateTreeExecutionContext& Context)
	{
		AActor* Owner = Cast<AActor>(Context.GetOwner());
		if (ACharacter* Direct = Cast<ACharacter>(Owner)) { return Direct; }
		if (AAIController* Controller = Cast<AAIController>(Owner)) { return Cast<ACharacter>(Controller->GetPawn()); }
		return nullptr;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> StandardSweepObjectTypes()
	{
		return {
			UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
			UEngineTypes::ConvertToObjectType(ECC_WorldDynamic),
			UEngineTypes::ConvertToObjectType(ECC_PhysicsBody)
		};
	}

	bool NeedsReturnHome(const AActor* Ghost)
	{
		if (!IsValid(Ghost)) { return false; }
		const FVector Home = ReadVector(Ghost, TEXT("HomeLocation"), Ghost->GetActorLocation());
		return FVector::Dist2D(Ghost->GetActorLocation(), Home) > 25.0;
	}

	bool IsInCooldown(const AActor* Ghost)
	{
		if (!IsValid(Ghost) || !Ghost->GetWorld()) { return false; }
		return Ghost->GetWorld()->GetTimeSeconds() < ReadDouble(Ghost, TEXT("CooldownUntil"), 0.0);
	}

	float GetVerticalDifficultySpeedMultiplier(const AActor* Ghost)
	{
		if (!IsValid(Ghost))
		{
			return 1.0f;
		}
		const double Z = Ghost->GetActorLocation().Z;
		if (Z >= 1450.0)
		{
			return 1.55f; // Floor 3 High-Risk Penthouse Tier 3
		}
		if (Z >= 600.0)
		{
			return 1.28f; // Floor 2 Mezzanine Tier 2
		}
		return 1.0f; // Ground Floor Tier 1
	}
}

void UMallGhostStateTreeLibrary::InitializeGhostAI(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character)) { return; }

	MallGhostAI::WriteString(Character, TEXT("PrototypeState"), TEXT("Idle"));
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 300.0f * MallGhostAI::GetVerticalDifficultySpeedMultiplier(Character);
		if (FBoolProperty* AccelProp = FindFProperty<FBoolProperty>(Movement->GetClass(), TEXT("bUseAccelerationForPaths")))
		{
			AccelProp->SetPropertyValue_InContainer(Movement, true);
		}
	}
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (!Character->HasAuthority()) { return; }

	AAIController* AIController = Cast<AAIController>(Character->GetController());
	if (!IsValid(AIController))
	{
		Character->SpawnDefaultController();
		AIController = Cast<AAIController>(Character->GetController());
	}
	MallGhostAI::WriteObject(Character, TEXT("GhostController"), AIController);

	FVector HomeLocation = MallGhostAI::ReadVector(Character, TEXT("HomeLocation"));
	if (HomeLocation.Size() <= 1.0)
	{
		HomeLocation = Character->GetActorLocation();
		MallGhostAI::WriteVector(Character, TEXT("HomeLocation"), HomeLocation);
	}

	bool bSafe = false;
	FVector NavFeet = FVector::ZeroVector;
	CheckSafeCenter(Character, HomeLocation, /*bRequireNav=*/true, bSafe, NavFeet);
	MallGhostAI::WriteBool(Character, TEXT("bHomeValid"), bSafe);
	MallGhostAI::WriteVector(Character, TEXT("HomeNavPoint"), NavFeet);
	if (bSafe)
	{
		MallGhostAI::WriteVector(Character, TEXT("LastSafeLocation"), HomeLocation);
		MallGhostAI::WriteBool(Character, TEXT("bHasLastSafe"), true);
	}
	else
	{
		MallGhostAI::WriteString(Character, TEXT("LastPathRejectReason"), TEXT("HomeLocation failed store or nav validation"));
		MallGhostAI::WriteInt(Character, TEXT("PathRejectCount"), MallGhostAI::ReadInt(Character, TEXT("PathRejectCount")) + 1);
	}
}

void UMallGhostStateTreeLibrary::CheckSafeCenter(AActor* Ghost, FVector Center, bool bRequireNav, bool& bSafe, FVector& NavFeet)
{
	bSafe = false;
	NavFeet = FVector::ZeroVector;
	ACharacter* Character = Cast<ACharacter>(Ghost);
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!IsValid(Character) || !IsValid(Capsule) || !Character->GetWorld() || Center.ContainsNaN())
	{
		return;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Feet = Center - FVector(0.0, 0.0, HalfHeight);
	const double StoreFloorZ = MallGhostAI::ReadDouble(Character, TEXT("StoreFloorZ"), 0.0);
	const bool bMultiFloorAnchor = Character->ActorHasTag(TEXT("MultiFloorAnchorGhost"));

	if (!UMallGhostCartHelpers::IsGhostCenterAllowed(Character, Center))
	{
		return;
	}
	if (!Character->ActorHasTag(TEXT("MallCartBoundary")) && !bMultiFloorAnchor
		&& FMath::Abs(Feet.Z - StoreFloorZ) > (bRequireNav ? 60.0 : 80.0))
	{
		return;
	}

	TArray<AActor*> ActorsToIgnore = { Character };
	if (AActor* TargetPlayer = MallGhostAI::ReadObject<AActor>(Character, TEXT("TargetPlayer")))
	{
		ActorsToIgnore.Add(TargetPlayer);
	}

	FHitResult HitResult;
	const bool bBlocked = UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		Character,
		Center + FVector(0.0, 0.0, 2.0),
		Center + FVector(0.0, 0.0, 3.0),
		Radius,
		HalfHeight,
		MallGhostAI::StandardSweepObjectTypes(),
		/*bTraceComplex=*/false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		/*bIgnoreSelf=*/true);
	if (bBlocked)
	{
		return;
	}

	if (!bRequireNav)
	{
		bSafe = true;
		NavFeet = Feet;
		return;
	}

	FVector ProjectedLocation = FVector::ZeroVector;
	const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
		Character,
		Feet,
		ProjectedLocation,
		nullptr,
		nullptr,
		FVector(25.0, 25.0, 60.0));
	if (!bProjected)
	{
		return;
	}

	if (UMallGhostCartHelpers::IsGhostFeetAllowed(Character, ProjectedLocation)
		&& (bMultiFloorAnchor || FMath::Abs(ProjectedLocation.Z - StoreFloorZ) <= 60.0)
		&& FVector::Dist(ProjectedLocation, Feet) <= 35.0)
	{
		bSafe = true;
		NavFeet = ProjectedLocation;
	}
}

void UMallGhostStateTreeLibrary::ClearRoute(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character)) { return; }

	MallGhostAI::WriteBool(Character, TEXT("HasValidatedPath"), false);
	if (AAIController* Controller = MallGhostAI::ReadObject<AAIController>(Character, TEXT("GhostController")))
	{
		Controller->StopMovement();
	}
	else if (AAIController* LiveController = Cast<AAIController>(Character->GetController()))
	{
		LiveController->StopMovement();
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (TArray<FVector>* Waypoints = MallGhostAI::GetVectorArrayPtr(Character, TEXT("CurrentValidWaypoints")))
	{
		Waypoints->Reset();
	}
	MallGhostAI::WriteInt(Character, TEXT("CurrentWaypointIndex"), 0);
	MallGhostAI::WriteBool(Character, TEXT("bWaypointMoveIssued"), false);
}

void UMallGhostStateTreeLibrary::RejectRoute(AActor* Ghost, const FString& Reason)
{
	if (!IsValid(Ghost)) { return; }
	ClearRoute(Ghost);
	MallGhostAI::WriteInt(Ghost, TEXT("PathRejectCount"), MallGhostAI::ReadInt(Ghost, TEXT("PathRejectCount")) + 1);
	MallGhostAI::WriteString(Ghost, TEXT("LastPathRejectReason"), Reason);
	MallGhostAI::WriteDouble(Ghost, TEXT("NextPlanTime"), 0.0);
}

void UMallGhostStateTreeLibrary::UpdateGuardAndEligibility(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character) || !Character->HasAuthority() || !Character->GetWorld()) { return; }

	AAIController* Controller = MallGhostAI::ReadObject<AAIController>(Character, TEXT("GhostController"));
	if (!IsValid(Controller))
	{
		Controller = Cast<AAIController>(Character->GetController());
		if (!IsValid(Controller))
		{
			Character->SpawnDefaultController();
			Controller = Cast<AAIController>(Character->GetController());
		}
		MallGhostAI::WriteObject(Character, TEXT("GhostController"), Controller);
	}

	UMallGhostCartHelpers::GuardCartGhostMovement(Character);

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const double RadiusNow = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.0;
	const double HalfHeightNow = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0;
	MallGhostAI::WriteDouble(Character, TEXT("CapsuleRadiusNow"), RadiusNow);
	MallGhostAI::WriteDouble(Character, TEXT("CapsuleHalfHeightNow"), HalfHeightNow);

	FVector HomeLocation = MallGhostAI::ReadVector(Character, TEXT("HomeLocation"));
	if (HomeLocation.Size() <= 1.0)
	{
		HomeLocation = Character->GetActorLocation();
		MallGhostAI::WriteVector(Character, TEXT("HomeLocation"), HomeLocation);
	}

	bool bHomeSafe = false;
	FVector HomeNav = FVector::ZeroVector;
	CheckSafeCenter(Character, HomeLocation, /*bRequireNav=*/true, bHomeSafe, HomeNav);
	MallGhostAI::WriteBool(Character, TEXT("bHomeValid"), bHomeSafe);
	MallGhostAI::WriteVector(Character, TEXT("HomeNavPoint"), HomeNav);
	if (!bHomeSafe)
	{
		MallGhostAI::WriteString(Character, TEXT("LastPathRejectReason"), TEXT("HomeLocation is outside store or off nav"));
	}

	bool bCurrentSafe = false;
	FVector IgnoredFeet = FVector::ZeroVector;
	const FVector CurrentLocation = Character->GetActorLocation();
	CheckSafeCenter(Character, CurrentLocation, /*bRequireNav=*/false, bCurrentSafe, IgnoredFeet);
	MallGhostAI::WriteBool(Character, TEXT("bPawnSafe"), bCurrentSafe);
	MallGhostAI::WriteVector(Character, TEXT("GhostFeet"), CurrentLocation - FVector(0.0, 0.0, HalfHeightNow));

	if (bCurrentSafe)
	{
		MallGhostAI::WriteVector(Character, TEXT("LastSafeLocation"), CurrentLocation);
		MallGhostAI::WriteBool(Character, TEXT("bHasLastSafe"), true);
	}
	else
	{
		MallGhostAI::WriteInt(Character, TEXT("OutsideBoundViolations"), MallGhostAI::ReadInt(Character, TEXT("OutsideBoundViolations")) + 1);
		RejectRoute(Character, TEXT("Ghost left store bounds or overlapped blocking geometry"));

		bool bRecovered = false;
		if (MallGhostAI::ReadBool(Character, TEXT("bHasLastSafe")))
		{
			const FVector LastSafe = MallGhostAI::ReadVector(Character, TEXT("LastSafeLocation"));
			bool bLastSafeOk = false;
			CheckSafeCenter(Character, LastSafe, /*bRequireNav=*/false, bLastSafeOk, IgnoredFeet);
			if (bLastSafeOk)
			{
				Character->SetActorLocation(LastSafe, false, nullptr, ETeleportType::TeleportPhysics);
				MallGhostAI::WriteBool(Character, TEXT("bPawnSafe"), true);
				MallGhostAI::WriteVector(Character, TEXT("GhostFeet"), LastSafe - FVector(0.0, 0.0, HalfHeightNow));
				bRecovered = true;
			}
		}
		if (!bRecovered && bHomeSafe)
		{
			Character->SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
			MallGhostAI::WriteVector(Character, TEXT("LastSafeLocation"), HomeLocation);
			MallGhostAI::WriteBool(Character, TEXT("bHasLastSafe"), true);
			MallGhostAI::WriteBool(Character, TEXT("bPawnSafe"), true);
			MallGhostAI::WriteVector(Character, TEXT("GhostFeet"), HomeLocation - FVector(0.0, 0.0, HalfHeightNow));
		}
	}

	ACharacter* TargetPlayer = UMallGhostCartHelpers::ResolveTargetPawn(Character);
	MallGhostAI::WriteObject(Character, TEXT("TargetPlayer"), TargetPlayer);
	if (IsValid(TargetPlayer))
	{
		const float PlayerHalfHeight = TargetPlayer->GetCapsuleComponent()
			? TargetPlayer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		MallGhostAI::WriteVector(Character, TEXT("PlayerFeet"), TargetPlayer->GetActorLocation() - FVector(0.0, 0.0, PlayerHalfHeight));
		MallGhostAI::WriteBool(Character, TEXT("bPlayerEligible"), UMallGhostCartHelpers::CanGhostPursuePlayer(Character, TargetPlayer));
	}
	else
	{
		MallGhostAI::WriteBool(Character, TEXT("bPlayerEligible"), false);
	}
}

void UMallGhostStateTreeLibrary::BuildValidatedRoute(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character) || !Character->HasAuthority() || !Character->GetWorld()) { return; }

	const double Now = Character->GetWorld()->GetTimeSeconds();
	MallGhostAI::WriteDouble(Character, TEXT("NextPlanTime"), Now + 0.35);
	MallGhostAI::WriteBool(Character, TEXT("bCandidateValid"), false);
	MallGhostAI::WriteString(Character, TEXT("RejectedReason"), TEXT(""));
	if (TArray<FVector>* CandidatePoints = MallGhostAI::GetVectorArrayPtr(Character, TEXT("CandidatePoints")))
	{
		CandidatePoints->Reset();
	}

	if (!MallGhostAI::ReadBool(Character, TEXT("bPawnSafe")) || !MallGhostAI::ReadBool(Character, TEXT("bHomeValid")))
	{
		RejectRoute(Character, TEXT("Cannot build route while ghost or home is invalid"));
		return;
	}

	bool bStartSafe = false;
	FVector StartNavFeet = FVector::ZeroVector;
	CheckSafeCenter(Character, Character->GetActorLocation(), /*bRequireNav=*/true, bStartSafe, StartNavFeet);
	MallGhostAI::WriteVector(Character, TEXT("GhostFeet"), StartNavFeet);
	if (!bStartSafe)
	{
		RejectRoute(Character, TEXT("Ghost start location is not on valid store navmesh"));
		return;
	}

	const FVector RequestedGoal = MallGhostAI::ReadVector(Character, TEXT("RequestedGoal"));
	FVector GoalProjected = FVector::ZeroVector;
	if (!UNavigationSystemV1::K2_ProjectPointToNavigation(Character, RequestedGoal, GoalProjected, nullptr, nullptr, FVector(35.0, 35.0, 80.0)))
	{
		RejectRoute(Character, TEXT("Requested goal could not project to store navmesh"));
		return;
	}

	const double HalfHeightNow = MallGhostAI::ReadDouble(Character, TEXT("CapsuleHalfHeightNow"), 90.0);
	const double RadiusNow = MallGhostAI::ReadDouble(Character, TEXT("CapsuleRadiusNow"), 34.0);

	bool bGoalSafe = false;
	FVector GoalNavFeet = FVector::ZeroVector;
	CheckSafeCenter(Character, GoalProjected + FVector(0.0, 0.0, HalfHeightNow), /*bRequireNav=*/true, bGoalSafe, GoalNavFeet);
	if (!bGoalSafe)
	{
		RejectRoute(Character, TEXT("Projected goal capsule is outside store or blocked"));
		return;
	}

	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(Character, StartNavFeet, GoalNavFeet, Character);
	MallGhostAI::WriteObject(Character, TEXT("CandidatePath"), Path);
	if (!IsValid(Path))
	{
		RejectRoute(Character, TEXT("Navigation path object was null"));
		return;
	}
	if (Path->IsPartial())
	{
		RejectRoute(Character, TEXT("Partial navigation path rejected"));
		return;
	}

	const TArray<FVector>& Points = Path->PathPoints;
	if (TArray<FVector>* CandidatePoints = MallGhostAI::GetVectorArrayPtr(Character, TEXT("CandidatePoints")))
	{
		*CandidatePoints = Points;
	}
	if (Points.Num() < 2)
	{
		RejectRoute(Character, TEXT("Navigation path had fewer than two points"));
		return;
	}
	if (FVector::Dist2D(Points.Last(), GoalNavFeet) > 35.0)
	{
		RejectRoute(Character, TEXT("Navigation path end did not match projected goal"));
		return;
	}

	TArray<AActor*> ActorsToIgnore = { Character };
	if (AActor* TargetPlayer = MallGhostAI::ReadObject<AActor>(Character, TEXT("TargetPlayer")))
	{
		ActorsToIgnore.Add(TargetPlayer);
	}

	const float SweepRadius = static_cast<float>(FMath::Max(RadiusNow - 2.0, 8.0));
	const float SweepHalfHeight = static_cast<float>(FMath::Max(HalfHeightNow - 10.0, 20.0));
	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = MallGhostAI::StandardSweepObjectTypes();

	FVector PreviousPoint = Points[0];
	MallGhostAI::WriteVector(Character, TEXT("PreviousPathPoint"), PreviousPoint);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector& Point = Points[Index];
		bool bPointSafe = false;
		FVector PointNavFeet = FVector::ZeroVector;
		CheckSafeCenter(Character, Point + FVector(0.0, 0.0, HalfHeightNow), /*bRequireNav=*/true, bPointSafe, PointNavFeet);
		if (!bPointSafe)
		{
			MallGhostAI::WriteString(Character, TEXT("RejectedReason"), TEXT("Path point failed store bounds, floor, nav, or capsule clearance"));
			RejectRoute(Character, TEXT("Path point failed store bounds, floor, nav, or capsule clearance"));
			return;
		}
		if (Index > 0)
		{
			FHitResult HitResult;
			const bool bHit = UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				Character,
				PreviousPoint + FVector(0.0, 0.0, HalfHeightNow),
				Point + FVector(0.0, 0.0, HalfHeightNow),
				SweepRadius,
				SweepHalfHeight,
				ObjectTypes,
				/*bTraceComplex=*/false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				HitResult,
				/*bIgnoreSelf=*/true);
			const bool bSegmentAllowed = UMallGhostCartHelpers::IsGhostRouteSegmentAllowed(Character, PreviousPoint, Point);
			if (bHit || !bSegmentAllowed)
			{
				MallGhostAI::WriteString(Character, TEXT("RejectedReason"), TEXT("Path segment swept into wall, door frame, or obstacle"));
				RejectRoute(Character, TEXT("Path segment swept into wall, door frame, or obstacle"));
				return;
			}
		}
		PreviousPoint = Point;
		MallGhostAI::WriteVector(Character, TEXT("PreviousPathPoint"), PreviousPoint);
	}

	if (TArray<FVector>* ValidWaypoints = MallGhostAI::GetVectorArrayPtr(Character, TEXT("CurrentValidWaypoints")))
	{
		*ValidWaypoints = Points;
	}
	MallGhostAI::WriteInt(Character, TEXT("CurrentWaypointIndex"), 0);
	MallGhostAI::WriteBool(Character, TEXT("HasValidatedPath"), true);
	MallGhostAI::WriteBool(Character, TEXT("bCandidateValid"), true);
	MallGhostAI::WriteVector(Character, TEXT("LastValidatedGoal"), GoalNavFeet);
	MallGhostAI::WriteString(Character, TEXT("LastPathRejectReason"), TEXT(""));
	MallGhostAI::WriteBool(Character, TEXT("bWaypointMoveIssued"), false);
	MallGhostAI::WriteVector(Character, TEXT("LastProgressLocation"), Character->GetActorLocation());
	MallGhostAI::WriteDouble(Character, TEXT("LastProgressTime"), Now);
}

void UMallGhostStateTreeLibrary::FollowValidatedRoute(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character) || !Character->HasAuthority() || !Character->GetWorld()) { return; }
	if (!MallGhostAI::ReadBool(Character, TEXT("HasValidatedPath"))) { return; }

	TArray<FVector>* Waypoints = MallGhostAI::GetVectorArrayPtr(Character, TEXT("CurrentValidWaypoints"));
	const int32 WaypointIndex = MallGhostAI::ReadInt(Character, TEXT("CurrentWaypointIndex"), 0);
	if (!Waypoints || !Waypoints->IsValidIndex(WaypointIndex))
	{
		ClearRoute(Character);
		return;
	}

	const FVector WaypointFeet = (*Waypoints)[WaypointIndex];
	if (!UMallGhostCartHelpers::CanFollowGhostWaypoint(Character, WaypointFeet))
	{
		RejectRoute(Character, TEXT("Cart or light protection changed"));
		return;
	}

	const FVector ActorLocation = Character->GetActorLocation();
	const double Now = Character->GetWorld()->GetTimeSeconds();
	if (FVector::Dist2D(ActorLocation, WaypointFeet) <= 10.0)
	{
		MallGhostAI::WriteInt(Character, TEXT("CurrentWaypointIndex"), WaypointIndex + 1);
		MallGhostAI::WriteBool(Character, TEXT("bWaypointMoveIssued"), false);
		MallGhostAI::WriteDouble(Character, TEXT("LastProgressTime"), Now);
		MallGhostAI::WriteVector(Character, TEXT("LastProgressLocation"), ActorLocation);
		return;
	}

	AAIController* Controller = MallGhostAI::ReadObject<AAIController>(Character, TEXT("GhostController"));
	if (!IsValid(Controller))
	{
		Controller = Cast<AAIController>(Character->GetController());
	}

	if (!MallGhostAI::ReadBool(Character, TEXT("bWaypointMoveIssued")))
	{
		if (!IsValid(Controller))
		{
			RejectRoute(Character, TEXT("Validated waypoint movement failed"));
			return;
		}
		const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToLocation(
			WaypointFeet,
			/*AcceptanceRadius=*/8.0f,
			/*bStopOnOverlap=*/false,
			/*bUsePathfinding=*/false,
			/*bProjectDestinationToNavigation=*/false,
			/*bCanStrafe=*/false,
			/*FilterClass=*/nullptr,
			/*bAllowPartialPaths=*/false);
		if (MoveResult == EPathFollowingRequestResult::Failed)
		{
			RejectRoute(Character, TEXT("Validated waypoint movement failed"));
			return;
		}
		MallGhostAI::WriteBool(Character, TEXT("bWaypointMoveIssued"), true);
		MallGhostAI::WriteDouble(Character, TEXT("LastProgressTime"), Now);
		MallGhostAI::WriteVector(Character, TEXT("LastProgressLocation"), ActorLocation);
		return;
	}

	if (!IsValid(Controller) || Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		RejectRoute(Character, TEXT("Waypoint stopped before it was reached"));
		return;
	}

	const FVector LastProgressLocation = MallGhostAI::ReadVector(Character, TEXT("LastProgressLocation"), ActorLocation);
	if (FVector::Dist2D(ActorLocation, LastProgressLocation) >= 5.0)
	{
		MallGhostAI::WriteDouble(Character, TEXT("LastProgressTime"), Now);
		MallGhostAI::WriteVector(Character, TEXT("LastProgressLocation"), ActorLocation);
	}
	else if (Now - MallGhostAI::ReadDouble(Character, TEXT("LastProgressTime"), Now) > 1.5)
	{
		RejectRoute(Character, TEXT("Validated waypoint stalled"));
	}
}

void UMallGhostStateTreeLibrary::TryCatchPlayer(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character) || !Character->HasAuthority() || !Character->GetWorld()) { return; }

	ACharacter* TargetPlayer = MallGhostAI::ReadObject<ACharacter>(Character, TEXT("TargetPlayer"));
	if (!UMallGhostCartHelpers::CanGhostCatchPlayer(Character, TargetPlayer)) { return; }

	if (MallGhostAI::ReadString(Character, TEXT("PrototypeState")) != TEXT("Chasing")
		|| !MallGhostAI::ReadBool(Character, TEXT("bPlayerEligible"))
		|| !MallGhostAI::ReadBool(Character, TEXT("bPawnSafe"))
		|| !MallGhostAI::ReadBool(Character, TEXT("bHomeValid"))
		|| !IsValid(TargetPlayer))
	{
		return;
	}

	const FVector GhostLocation = Character->GetActorLocation();
	const FVector PlayerLocation = TargetPlayer->GetActorLocation();
	if (FVector::Dist(GhostLocation, PlayerLocation) > 80.0)
	{
		return;
	}

	const TArray<AActor*> ActorsToIgnore = { Character, TargetPlayer };
	FHitResult HitResult;
	const bool bBlocked = UKismetSystemLibrary::LineTraceSingleForObjects(
		Character,
		GhostLocation,
		PlayerLocation,
		MallGhostAI::StandardSweepObjectTypes(),
		/*bTraceComplex=*/false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		/*bIgnoreSelf=*/true);
	if (bBlocked)
	{
		return;
	}

	const FVector HomeLocation = MallGhostAI::ReadVector(Character, TEXT("HomeLocation"));
	bool bHomeSafe = false;
	FVector IgnoredNavFeet = FVector::ZeroVector;
	CheckSafeCenter(Character, HomeLocation, /*bRequireNav=*/true, bHomeSafe, IgnoredNavFeet);
	if (!bHomeSafe)
	{
		return;
	}

	ClearRoute(Character);
	MallGhostAI::WriteString(Character, TEXT("PrototypeState"), TEXT("Cooldown"));
	MallGhostAI::WriteInt(Character, TEXT("CatchCount"), MallGhostAI::ReadInt(Character, TEXT("CatchCount")) + 1);
	if (UMallPlayerVitalityComponent* Vit = UMallPlayerVitalityComponent::FindOrCreateForPawn(TargetPlayer))
	{
		Vit->ApplyGhostChillDamage(32.0f * MallGhostAI::GetVerticalDifficultySpeedMultiplier(Character));
	}
	Character->SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
	MallGhostAI::WriteDouble(Character, TEXT("CooldownUntil"), Character->GetWorld()->GetTimeSeconds() + 3.0);
	UKismetSystemLibrary::PrintString(Character, TEXT("Caught - ghost reset (visit South Spa Salt Baths or Food Court to recover!)"), true, true, FLinearColor(0.0f, 0.66f, 1.0f, 1.0f), 3.0f);
}

void UMallGhostStateTreeLibrary::RuntimeStateTick(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character) || !Character->HasAuthority() || !Character->GetWorld()) { return; }

	UpdateGuardAndEligibility(Character);
	if (!UMallGhostInteractionComponent::CanGhostHunt(Character))
	{
		return;
	}

	const double Now = Character->GetWorld()->GetTimeSeconds();
	if (Now < MallGhostAI::ReadDouble(Character, TEXT("CooldownUntil"), 0.0))
	{
		MallGhostAI::WriteString(Character, TEXT("PrototypeState"), TEXT("Cooldown"));
		ClearRoute(Character);
		return;
	}

	if (MallGhostAI::ReadBool(Character, TEXT("bPlayerEligible")))
	{
		MallGhostAI::WriteString(Character, TEXT("PrototypeState"), TEXT("Chasing"));
		const FVector PlayerFeet = MallGhostAI::ReadVector(Character, TEXT("PlayerFeet"));
		MallGhostAI::WriteVector(Character, TEXT("RequestedGoal"), PlayerFeet);
		if (!MallGhostAI::ReadBool(Character, TEXT("HasValidatedPath"))
			|| Now >= MallGhostAI::ReadDouble(Character, TEXT("NextPlanTime"), 0.0)
			|| FVector::Dist2D(PlayerFeet, MallGhostAI::ReadVector(Character, TEXT("LastValidatedGoal"))) > 75.0)
		{
			BuildValidatedRoute(Character);
		}
		FollowValidatedRoute(Character);
		TryCatchPlayer(Character);
		return;
	}

	const FVector HomeLocation = MallGhostAI::ReadVector(Character, TEXT("HomeLocation"));
	if (FVector::Dist2D(Character->GetActorLocation(), HomeLocation) <= 25.0)
	{
		MallGhostAI::WriteString(Character, TEXT("PrototypeState"), TEXT("Idle"));
		ClearRoute(Character);
		return;
	}

	MallGhostAI::WriteString(Character, TEXT("PrototypeState"), TEXT("Returning"));
	const FVector HomeNavPoint = MallGhostAI::ReadVector(Character, TEXT("HomeNavPoint"));
	MallGhostAI::WriteVector(Character, TEXT("RequestedGoal"), HomeNavPoint);
	if (!MallGhostAI::ReadBool(Character, TEXT("HasValidatedPath"))
		|| Now >= MallGhostAI::ReadDouble(Character, TEXT("NextPlanTime"), 0.0)
		|| FVector::Dist2D(HomeNavPoint, MallGhostAI::ReadVector(Character, TEXT("LastValidatedGoal"))) > 25.0)
	{
		BuildValidatedRoute(Character);
	}
	FollowValidatedRoute(Character);
}

void UMallGhostStateTreeLibrary::TickGhostWithStateTreeFallback(AActor* Ghost)
{
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!IsValid(Character) || !Character->HasAuthority()) { return; }

	if (const AAIController* Controller = Cast<AAIController>(Character->GetController()))
	{
		if (const UStateTreeAIComponent* StateTreeComp = Controller->FindComponentByClass<UStateTreeAIComponent>())
		{
			if (StateTreeComp->IsRunning())
			{
				return;
			}
		}
	}
	RuntimeStateTick(Character);
}

void FMallGhostStateTreeEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled()) { return; }
	UMallGhostStateTreeLibrary::InitializeGhostAI(Ghost);
}

void FMallGhostStateTreeEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMallGhostStateTreeEvaluatorInstanceData& InstanceData = Context.GetInstanceData(*this);
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	InstanceData.GhostCharacter = Ghost;
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled())
	{
		return;
	}

	UMallGhostStateTreeLibrary::UpdateGuardAndEligibility(Ghost);
	InstanceData.TargetPlayer = MallGhostAI::ReadObject<ACharacter>(Ghost, TEXT("TargetPlayer"));
	InstanceData.bCanHunt = UMallGhostInteractionComponent::CanGhostHunt(Ghost);
	InstanceData.bInCooldown = MallGhostAI::IsInCooldown(Ghost);
	InstanceData.bPlayerEligible = MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible"));
	InstanceData.bNeedsReturnHome = MallGhostAI::NeedsReturnHome(Ghost);
	InstanceData.PrototypeState = MallGhostAI::ReadString(Ghost, TEXT("PrototypeState"));
}

bool FMallGhostCanHuntCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	const bool bResult = IsValid(Ghost) && UMallGhostInteractionComponent::CanGhostHunt(Ghost);
	return bResult ^ bInvert;
}

bool FMallGhostInCooldownCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	const bool bResult = MallGhostAI::IsInCooldown(Ghost);
	return bResult ^ bInvert;
}

bool FMallGhostPlayerEligibleCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	const bool bResult = IsValid(Ghost) && MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible"));
	return bResult ^ bInvert;
}

bool FMallGhostNeedsReturnHomeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	const bool bResult = MallGhostAI::NeedsReturnHome(Ghost);
	return bResult ^ bInvert;
}

EStateTreeRunStatus FMallGhostDormantTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostDormantTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled())
	{
		return EStateTreeRunStatus::Running;
	}
	if (UMallGhostInteractionComponent::CanGhostHunt(Ghost))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostCooldownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (IsValid(Ghost) && Ghost->HasAuthority() && Ghost->IsActorTickEnabled())
	{
		MallGhostAI::WriteString(Ghost, TEXT("PrototypeState"), TEXT("Cooldown"));
		UMallGhostStateTreeLibrary::ClearRoute(Ghost);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostCooldownTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost) || !MallGhostAI::IsInCooldown(Ghost))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	MallGhostAI::WriteString(Ghost, TEXT("PrototypeState"), TEXT("Cooldown"));
	UMallGhostStateTreeLibrary::ClearRoute(Ghost);
	return EStateTreeRunStatus::Running;
}

namespace MallGhostStateTreeStep
{
	void ExecuteChaseStep(ACharacter* Ghost)
	{
		const double Now = Ghost->GetWorld()->GetTimeSeconds();
		MallGhostAI::WriteString(Ghost, TEXT("PrototypeState"), TEXT("Chasing"));
		const FVector PlayerFeet = MallGhostAI::ReadVector(Ghost, TEXT("PlayerFeet"));
		MallGhostAI::WriteVector(Ghost, TEXT("RequestedGoal"), PlayerFeet);
		if (!MallGhostAI::ReadBool(Ghost, TEXT("HasValidatedPath"))
			|| Now >= MallGhostAI::ReadDouble(Ghost, TEXT("NextPlanTime"), 0.0)
			|| FVector::Dist2D(PlayerFeet, MallGhostAI::ReadVector(Ghost, TEXT("LastValidatedGoal"))) > 75.0)
		{
			UMallGhostStateTreeLibrary::BuildValidatedRoute(Ghost);
		}
		UMallGhostStateTreeLibrary::FollowValidatedRoute(Ghost);
		UMallGhostStateTreeLibrary::TryCatchPlayer(Ghost);
	}

	void ExecuteReturnStep(ACharacter* Ghost)
	{
		const double Now = Ghost->GetWorld()->GetTimeSeconds();
		MallGhostAI::WriteString(Ghost, TEXT("PrototypeState"), TEXT("Returning"));
		const FVector HomeNavPoint = MallGhostAI::ReadVector(Ghost, TEXT("HomeNavPoint"));
		MallGhostAI::WriteVector(Ghost, TEXT("RequestedGoal"), HomeNavPoint);
		if (!MallGhostAI::ReadBool(Ghost, TEXT("HasValidatedPath"))
			|| Now >= MallGhostAI::ReadDouble(Ghost, TEXT("NextPlanTime"), 0.0)
			|| FVector::Dist2D(HomeNavPoint, MallGhostAI::ReadVector(Ghost, TEXT("LastValidatedGoal"))) > 25.0)
		{
			UMallGhostStateTreeLibrary::BuildValidatedRoute(Ghost);
		}
		UMallGhostStateTreeLibrary::FollowValidatedRoute(Ghost);
	}
}

EStateTreeRunStatus FMallGhostChaseTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (IsValid(Ghost) && Ghost->HasAuthority() && Ghost->IsActorTickEnabled() && Ghost->GetWorld()
		&& UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		&& !MallGhostAI::IsInCooldown(Ghost)
		&& MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible")))
	{
		MallGhostStateTreeStep::ExecuteChaseStep(Ghost);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostChaseTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled() || !Ghost->GetWorld())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		|| MallGhostAI::IsInCooldown(Ghost)
		|| !MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible")))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	MallGhostStateTreeStep::ExecuteChaseStep(Ghost);
	if (MallGhostAI::IsInCooldown(Ghost))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostReturnHomeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (IsValid(Ghost) && Ghost->HasAuthority() && Ghost->IsActorTickEnabled() && Ghost->GetWorld()
		&& UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		&& !MallGhostAI::IsInCooldown(Ghost)
		&& !MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible"))
		&& MallGhostAI::NeedsReturnHome(Ghost))
	{
		MallGhostStateTreeStep::ExecuteReturnStep(Ghost);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostReturnHomeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled() || !Ghost->GetWorld())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		|| MallGhostAI::IsInCooldown(Ghost)
		|| MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible"))
		|| !MallGhostAI::NeedsReturnHome(Ghost))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	MallGhostStateTreeStep::ExecuteReturnStep(Ghost);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostIdleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (IsValid(Ghost) && Ghost->HasAuthority() && Ghost->IsActorTickEnabled()
		&& UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		&& !MallGhostAI::IsInCooldown(Ghost)
		&& !MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible"))
		&& !MallGhostAI::NeedsReturnHome(Ghost))
	{
		MallGhostAI::WriteString(Ghost, TEXT("PrototypeState"), TEXT("Idle"));
		UMallGhostStateTreeLibrary::ClearRoute(Ghost);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMallGhostIdleTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ACharacter* Ghost = MallGhostAI::ResolveGhostCharacter(Context);
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->IsActorTickEnabled())
	{
		return EStateTreeRunStatus::Running;
	}
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		|| MallGhostAI::IsInCooldown(Ghost)
		|| MallGhostAI::ReadBool(Ghost, TEXT("bPlayerEligible"))
		|| MallGhostAI::NeedsReturnHome(Ghost))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	MallGhostAI::WriteString(Ghost, TEXT("PrototypeState"), TEXT("Idle"));
	UMallGhostStateTreeLibrary::ClearRoute(Ghost);
	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
namespace MallGhostBlueprintSimplifier
{
	UK2Node_Self* AddSelfNode(UEdGraph* Graph, const int32 X, const int32 Y)
	{
		UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
		Graph->AddNode(SelfNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);
		SelfNode->CreateNewGuid();
		SelfNode->PostPlacedNewNode();
		SelfNode->AllocateDefaultPins();
		SelfNode->NodePosX = X;
		SelfNode->NodePosY = Y;
		return SelfNode;
	}

	UK2Node_CallFunction* AddLibraryCallNode(UEdGraph* Graph, const FName FunctionName, const int32 X, const int32 Y)
	{
		UFunction* Function = UMallGhostStateTreeLibrary::StaticClass()->FindFunctionByName(FunctionName);
		if (!Function) { return nullptr; }
		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		CallNode->SetFromFunction(Function);
		Graph->AddNode(CallNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);
		CallNode->CreateNewGuid();
		CallNode->PostPlacedNewNode();
		CallNode->AllocateDefaultPins();
		CallNode->NodePosX = X;
		CallNode->NodePosY = Y;
		return CallNode;
	}

	void SimplifyStoreGhostBlueprint(UBlueprint* GhostBP)
	{
		if (!GhostBP) { return; }

		for (UEdGraph* Graph : GhostBP->FunctionGraphs)
		{
			if (!Graph || Graph->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript) { continue; }
			const FName GraphName = Graph->GetFName();

			UK2Node_FunctionEntry* EntryNode = nullptr;
			UK2Node_FunctionResult* KeptResultNode = nullptr;
			TArray<UEdGraphNode*> NodesToRemove;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) { continue; }
				if (!EntryNode && Node->IsA<UK2Node_FunctionEntry>())
				{
					EntryNode = Cast<UK2Node_FunctionEntry>(Node);
				}
				else if (!KeptResultNode && GraphName == TEXT("CheckSafeCenter") && Node->IsA<UK2Node_FunctionResult>())
				{
					KeptResultNode = Cast<UK2Node_FunctionResult>(Node);
				}
				else
				{
					NodesToRemove.Add(Node);
				}
			}
			for (UEdGraphNode* Node : NodesToRemove)
			{
				FBlueprintEditorUtils::RemoveNode(GhostBP, Node, /*bDontRecompile=*/true);
			}
			if (!EntryNode) { continue; }
			for (UEdGraphPin* Pin : EntryNode->Pins) { if (Pin) { Pin->BreakAllPinLinks(); } }
			if (KeptResultNode)
			{
				for (UEdGraphPin* Pin : KeptResultNode->Pins) { if (Pin) { Pin->BreakAllPinLinks(); } }
				KeptResultNode->NodePosX = 750;
				KeptResultNode->NodePosY = 0;
			}

			UK2Node_Self* SelfNode = AddSelfNode(Graph, 150, 140);
			UK2Node_CallFunction* CallNode = AddLibraryCallNode(Graph, GraphName, 360, 0);
			if (!SelfNode || !CallNode) { continue; }

			if (UEdGraphPin* EntryThen = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then))
			{
				EntryThen->MakeLinkTo(CallNode->GetExecPin());
			}
			if (UEdGraphPin* SelfPin = SelfNode->FindPin(UEdGraphSchema_K2::PN_Self))
			{
				if (UEdGraphPin* GhostPin = CallNode->FindPin(TEXT("Ghost")))
				{
					SelfPin->MakeLinkTo(GhostPin);
				}
			}

			if (GraphName == TEXT("RejectRoute"))
			{
				if (UEdGraphPin* InReason = EntryNode->FindPin(TEXT("Reason")))
				{
					if (UEdGraphPin* CallReason = CallNode->FindPin(TEXT("Reason")))
					{
						InReason->MakeLinkTo(CallReason);
					}
				}
			}
			else if (GraphName == TEXT("CheckSafeCenter") && KeptResultNode)
			{
				if (UEdGraphPin* CallThen = CallNode->GetThenPin())
				{
					CallThen->MakeLinkTo(KeptResultNode->GetExecPin());
				}
				if (UEdGraphPin* InCenter = EntryNode->FindPin(TEXT("Center")))
				{
					if (UEdGraphPin* CallCenter = CallNode->FindPin(TEXT("Center"))) { InCenter->MakeLinkTo(CallCenter); }
				}
				if (UEdGraphPin* InRequireNav = EntryNode->FindPin(TEXT("RequireNav")))
				{
					if (UEdGraphPin* CallRequireNav = CallNode->FindPin(TEXT("bRequireNav"))) { InRequireNav->MakeLinkTo(CallRequireNav); }
				}
				if (UEdGraphPin* CallSafe = CallNode->FindPin(TEXT("bSafe")))
				{
					if (UEdGraphPin* OutSafe = KeptResultNode->FindPin(TEXT("Safe"))) { CallSafe->MakeLinkTo(OutSafe); }
				}
				if (UEdGraphPin* CallNavFeet = CallNode->FindPin(TEXT("NavFeet")))
				{
					if (UEdGraphPin* OutNavFeet = KeptResultNode->FindPin(TEXT("NavFeet"))) { CallNavFeet->MakeLinkTo(OutNavFeet); }
				}
			}
		}

		for (UEdGraph* EventGraph : GhostBP->UbergraphPages)
		{
			if (!EventGraph) { continue; }
			UK2Node_Event* BeginPlayEvent = nullptr;
			UK2Node_Event* TickEvent = nullptr;
			TArray<UEdGraphNode*> NodesToRemove;
			for (UEdGraphNode* Node : EventGraph->Nodes)
			{
				UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
				const FName EventName = EventNode ? EventNode->EventReference.GetMemberName() : NAME_None;
				if (!BeginPlayEvent && EventName == TEXT("ReceiveBeginPlay"))
				{
					BeginPlayEvent = EventNode;
				}
				else if (!TickEvent && EventName == TEXT("ReceiveTick"))
				{
					TickEvent = EventNode;
				}
				else if (Node)
				{
					NodesToRemove.Add(Node);
				}
			}
			for (UEdGraphNode* Node : NodesToRemove)
			{
				FBlueprintEditorUtils::RemoveNode(GhostBP, Node, /*bDontRecompile=*/true);
			}

			UK2Node_Self* SelfNode = AddSelfNode(EventGraph, 120, 120);
			if (BeginPlayEvent)
			{
				for (UEdGraphPin* Pin : BeginPlayEvent->Pins) { if (Pin) { Pin->BreakAllPinLinks(); } }
				BeginPlayEvent->NodePosX = 0;
				BeginPlayEvent->NodePosY = 0;
				if (UK2Node_CallFunction* InitCall = AddLibraryCallNode(EventGraph, TEXT("InitializeGhostAI"), 360, 0))
				{
					if (UEdGraphPin* ThenPin = BeginPlayEvent->FindPin(UEdGraphSchema_K2::PN_Then))
					{
						ThenPin->MakeLinkTo(InitCall->GetExecPin());
					}
					if (SelfNode)
					{
						if (UEdGraphPin* GhostPin = InitCall->FindPin(TEXT("Ghost")))
						{
							SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self)->MakeLinkTo(GhostPin);
						}
					}
				}
			}
			if (TickEvent)
			{
				for (UEdGraphPin* Pin : TickEvent->Pins) { if (Pin) { Pin->BreakAllPinLinks(); } }
				TickEvent->NodePosX = 0;
				TickEvent->NodePosY = 220;
				if (UK2Node_CallFunction* TickCall = AddLibraryCallNode(EventGraph, TEXT("TickGhostWithStateTreeFallback"), 360, 220))
				{
					if (UEdGraphPin* ThenPin = TickEvent->FindPin(UEdGraphSchema_K2::PN_Then))
					{
						ThenPin->MakeLinkTo(TickCall->GetExecPin());
					}
					if (SelfNode)
					{
						if (UEdGraphPin* GhostPin = TickCall->FindPin(TEXT("Ghost")))
						{
							SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self)->MakeLinkTo(GhostPin);
						}
					}
				}
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GhostBP);
		FKismetEditorUtilities::CompileBlueprint(GhostBP);
	}
}
#endif

bool UMallGhostStateTreeLibrary::BuildStoreGhostStateTreeAsset()
{
#if WITH_EDITOR
	const FString PackagePath = TEXT("/Game/AI/StoreGhost/ST_StoreGhost");
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) { return false; }
	Package->FullyLoad();

	UStateTree* StateTree = FindObject<UStateTree>(Package, TEXT("ST_StoreGhost"));
	const bool bCreatedNew = (StateTree == nullptr);
	if (!StateTree)
	{
		StateTree = NewObject<UStateTree>(Package, TEXT("ST_StoreGhost"), RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!StateTree) { return false; }

	FStateTreeEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FStateTreeEditorModule>(TEXT("StateTreeEditorModule"));
	TNonNullSubclassOf<UStateTreeEditorData> EditorDataClass = EditorModule.GetEditorDataClass(UStateTreeAIComponentSchema::StaticClass());
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree, EditorDataClass, NAME_None, RF_Transactional);
	StateTree->EditorData = EditorData;

	UStateTreeAIComponentSchema* Schema = NewObject<UStateTreeAIComponentSchema>(EditorData, NAME_None, RF_Transactional);
	EditorData->Schema = Schema;

	UClass* ControllerClass = LoadClass<AAIController>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhostController.BP_StoreGhostController_C"));
	if (FClassProperty* ContextActorProp = FindFProperty<FClassProperty>(UStateTreeComponentSchema::StaticClass(), TEXT("ContextActorClass")))
	{
		ContextActorProp->SetObjectPropertyValue_InContainer(Schema, AActor::StaticClass());
	}
	if (FClassProperty* AIControllerProp = FindFProperty<FClassProperty>(UStateTreeAIComponentSchema::StaticClass(), TEXT("AIControllerClass")))
	{
		AIControllerProp->SetObjectPropertyValue_InContainer(Schema, ControllerClass ? ControllerClass : AAIController::StaticClass());
	}
	if (FEnumProperty* TickPolicyProp = FindFProperty<FEnumProperty>(UStateTreeComponentSchema::StaticClass(), TEXT("ScheduledTickPolicy")))
	{
		const uint8 AllowedVal = static_cast<uint8>(EStateTreeComponentSchemaScheduledTickPolicy::Allowed);
		TickPolicyProp->GetUnderlyingProperty()->SetIntPropertyValue(TickPolicyProp->ContainerPtrToValuePtr<void>(Schema), static_cast<int64>(AllowedVal));
	}
	Schema->PostLoad();

	TNonNullSubclassOf<UStateTreeEditorSchema> EditorSchemaClass = EditorModule.GetEditorSchemaClass(UStateTreeAIComponentSchema::StaticClass());
	EditorData->EditorSchema = NewObject<UStateTreeEditorSchema>(EditorData, EditorSchemaClass, NAME_None, RF_Transactional);

	auto& Eval = EditorData->AddEvaluator<FMallGhostStateTreeEvaluator>();
	Eval.GetNode().Name = FName(TEXT("Mall Ghost Guard & Sensor Evaluator"));

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.SelectionBehavior = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;

	// 1. Dormant / Invisible / Warning
	UStateTreeState& DormantState = Root.AddChildState(FName(TEXT("DormantOrInvisible")), EStateTreeStateType::State);
	auto& DormantCond = DormantState.AddEnterCondition<FMallGhostCanHuntCondition>();
	DormantCond.GetNode().bInvert = true;
	auto& DormantTask = DormantState.AddTask<FMallGhostDormantTask>();
	DormantTask.GetNode().Name = FName(TEXT("Maintain Dormant Or Warning State"));
	DormantState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);

	// 2. Post-Catch / Voice Cooldown
	UStateTreeState& CooldownState = Root.AddChildState(FName(TEXT("Cooldown")), EStateTreeStateType::State);
	CooldownState.AddEnterCondition<FMallGhostInCooldownCondition>();
	auto& CooldownTask = CooldownState.AddTask<FMallGhostCooldownTask>();
	CooldownTask.GetNode().Name = FName(TEXT("Post-Catch Or Voice Cooldown"));
	CooldownState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);

	// 3. Chasing Eligible Player
	UStateTreeState& ChasingState = Root.AddChildState(FName(TEXT("Chasing")), EStateTreeStateType::State);
	ChasingState.AddEnterCondition<FMallGhostPlayerEligibleCondition>();
	auto& ChaseTask = ChasingState.AddTask<FMallGhostChaseTask>();
	ChaseTask.GetNode().Name = FName(TEXT("Validated Confinement Chase & Catch"));
	ChasingState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);

	// 4. Returning To Store Home
	UStateTreeState& ReturningState = Root.AddChildState(FName(TEXT("ReturningHome")), EStateTreeStateType::State);
	ReturningState.AddEnterCondition<FMallGhostNeedsReturnHomeCondition>();
	auto& ReturnTask = ReturningState.AddTask<FMallGhostReturnHomeTask>();
	ReturnTask.GetNode().Name = FName(TEXT("Validated Return To Home Location"));
	ReturningState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);

	// 5. Idle At Store Home
	UStateTreeState& IdleState = Root.AddChildState(FName(TEXT("Idle")), EStateTreeStateType::State);
	auto& IdleTask = IdleState.AddTask<FMallGhostIdleTask>();
	IdleTask.GetNode().Name = FName(TEXT("Idle At Home & Await Target"));
	IdleState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::GotoState, &Root);

	UStateTreeEditingSubsystem::ValidateStateTree(StateTree);
	FStateTreeCompilerLog Log;
	const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(StateTree, Log);
	if (!bCompiled)
	{
		UE_LOG(LogMallGhostStateTree, Error, TEXT("Failed to compile ST_StoreGhost."));
		return false;
	}

	if (bCreatedNew)
	{
		FAssetRegistryModule::AssetCreated(StateTree);
	}
	Package->MarkPackageDirty();
	const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, StateTree, *FileName, SaveArgs);

	// Wire BP_StoreGhostController with StateTreeAI component referencing ST_StoreGhost
	if (UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhostController.BP_StoreGhostController")))
	{
		if (USimpleConstructionScript* SCS = ControllerBP->SimpleConstructionScript)
		{
			USCS_Node* StateTreeNode = nullptr;
			for (USCS_Node* Node : SCS->GetAllNodes())
			{
				if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(UStateTreeAIComponent::StaticClass()))
				{
					StateTreeNode = Node;
					break;
				}
			}
			if (!StateTreeNode)
			{
				StateTreeNode = SCS->CreateNode(UStateTreeAIComponent::StaticClass(), TEXT("StateTreeAI"));
				SCS->AddNode(StateTreeNode);
			}
			if (StateTreeNode)
			{
				if (UStateTreeAIComponent* CompTemplate = Cast<UStateTreeAIComponent>(StateTreeNode->ComponentTemplate))
				{
					if (FStructProperty* RefProp = FindFProperty<FStructProperty>(UStateTreeComponent::StaticClass(), TEXT("StateTreeRef")))
					{
						if (FStateTreeReference* RefPtr = RefProp->ContainerPtrToValuePtr<FStateTreeReference>(CompTemplate))
						{
							RefPtr->SetStateTree(StateTree);
						}
					}
					CompTemplate->SetStartLogicAutomatically(true);
				}
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ControllerBP);
		FKismetEditorUtilities::CompileBlueprint(ControllerBP);
		if (ControllerBP->GeneratedClass)
		{
			if (AAIController* ControllerCDO = Cast<AAIController>(ControllerBP->GeneratedClass->GetDefaultObject()))
			{
				if (FBoolProperty* StartProp = FindFProperty<FBoolProperty>(AAIController::StaticClass(), TEXT("bStartAILogicOnPossess")))
				{
					StartProp->SetPropertyValue_InContainer(ControllerCDO, true);
				}
			}
			FKismetEditorUtilities::CompileBlueprint(ControllerBP);
		}
		UPackage* ControllerPkg = ControllerBP->GetOutermost();
		ControllerPkg->MarkPackageDirty();
		const FString ControllerFile = FPackageName::LongPackageNameToFilename(ControllerPkg->GetName(), FPackageName::GetAssetPackageExtension());
		UPackage::SavePackage(ControllerPkg, ControllerBP, *ControllerFile, SaveArgs);
	}

	// Simplify BP_StoreGhost graphs to clean 1-node C++ delegates
	if (UBlueprint* GhostBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost")))
	{
		MallGhostBlueprintSimplifier::SimplifyStoreGhostBlueprint(GhostBP);
		UPackage* GhostPkg = GhostBP->GetOutermost();
		GhostPkg->MarkPackageDirty();
		const FString GhostFile = FPackageName::LongPackageNameToFilename(GhostPkg->GetName(), FPackageName::GetAssetPackageExtension());
		UPackage::SavePackage(GhostPkg, GhostBP, *GhostFile, SaveArgs);
	}

	return true;
#else
	return false;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallGhostStateTreeAssetAndExecutionTest, "Mall.Ghost.StateTreeAssetAndExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostStateTreeAssetAndExecutionTest::RunTest(const FString& Parameters)
{
	UMallVoiceSettings* Settings = GetMutableDefault<UMallVoiceSettings>();
	const bool bSavedVoice = Settings->bEnabled;
	const bool bSavedSpeech = Settings->bSpeechRecognitionEnabled;
	const bool bSavedStatus = Settings->bShowVoiceStatus;
	const FString SavedMap = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
	const FString SavedMode = UGameMapsSettings::GetGlobalDefaultGameMode();
	const TSubclassOf<UGameViewportClient> SavedViewport = GEngine->GameViewportClientClass;
	ON_SCOPE_EXIT
	{
		Settings->bEnabled = bSavedVoice;
		Settings->bSpeechRecognitionEnabled = bSavedSpeech;
		Settings->bShowVoiceStatus = bSavedStatus;
		UGameMapsSettings::SetGameDefaultMap(SavedMap);
		UGameMapsSettings::SetGlobalDefaultGameMode(SavedMode);
		GEngine->GameViewportClientClass = SavedViewport;
	};
	Settings->bEnabled = Settings->bSpeechRecognitionEnabled = Settings->bShowVoiceStatus = false;
	UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
	UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
	if (UClass* Compatible = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient")))
	{
		GEngine->GameViewportClientClass = Compatible;
	}

	UStateTree* StateTree = LoadObject<UStateTree>(nullptr, TEXT("/Game/AI/StoreGhost/ST_StoreGhost.ST_StoreGhost"));
	const UStateTreeComponentSchema* ExistingSchema = StateTree ? Cast<UStateTreeComponentSchema>(StateTree->GetSchema()) : nullptr;
	if (!StateTree || !StateTree->IsReadyToRun() || !ExistingSchema || ExistingSchema->GetContextActorClass() != AActor::StaticClass())
	{
		TestTrue(TEXT("StateTree builder creates and compiles ST_StoreGhost"), UMallGhostStateTreeLibrary::BuildStoreGhostStateTreeAsset());
		StateTree = LoadObject<UStateTree>(nullptr, TEXT("/Game/AI/StoreGhost/ST_StoreGhost.ST_StoreGhost"));
	}
	if (!TestNotNull(TEXT("ST_StoreGhost StateTree asset exists"), StateTree)) { return false; }
	TestTrue(TEXT("ST_StoreGhost is compiled and ready to run"), StateTree->IsReadyToRun());
	TestTrue(TEXT("ST_StoreGhost uses StateTreeAIComponentSchema"),
		StateTree->GetSchema() && StateTree->GetSchema()->IsA<UStateTreeAIComponentSchema>());

	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Server world loads"), Worlds.Server.IsLoaded())) { return false; }
	UWorld* Server = Worlds.Server.GetWorld();

	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("BP_StoreGhost loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(5050.0, 100.0, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("BP_StoreGhost spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype"));
	MallGhostAI::WriteDouble(Ghost, TEXT("StoreMinX"), 3950.0);
	MallGhostAI::WriteDouble(Ghost, TEXT("StoreMaxX"), 5950.0);
	MallGhostAI::WriteDouble(Ghost, TEXT("StoreMinY"), -816.6666667);
	MallGhostAI::WriteDouble(Ghost, TEXT("StoreMaxY"), 816.6666667);
	MallGhostAI::WriteDouble(Ghost, TEXT("StoreFloorZ"), 0.0);
	MallGhostAI::WriteDouble(Ghost, TEXT("CooldownUntil"), 0.0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home);
	Ghost->SetActorTransform(Home);
	Ghost->GetCharacterMovement()->DisableMovement();

	AAIController* Controller = Cast<AAIController>(Ghost->GetController());
	if (!TestNotNull(TEXT("BP_StoreGhostController possesses BP_StoreGhost"), Controller)) { return false; }
	UStateTreeAIComponent* StateTreeComp = Controller->FindComponentByClass<UStateTreeAIComponent>();
	if (!TestNotNull(TEXT("BP_StoreGhostController has StateTreeAIComponent"), StateTreeComp)) { return false; }
	if (!StateTreeComp->IsRunning())
	{
		StateTreeComp->StartLogic();
	}
	TestTrue(TEXT("StateTreeAIComponent is actively running ST_StoreGhost"), StateTreeComp->IsRunning());

	Worlds.TickAll(5);
	UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	if (!TestNotNull(TEXT("MallGhostInteractionComponent attached"), Interaction)) { return false; }
	Interaction->bAutomaticHunts = false;
	Interaction->bAutomaticDisturbances = false;
	if (UMallGhostSoundComponent* Sound = Ghost->FindComponentByClass<UMallGhostSoundComponent>())
	{
		Sound->bAutomaticSounds = false;
	}

	// 1. In InvisibleDisturbance mode, StateTree stays in DormantOrInvisible
	Interaction->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	Worlds.TickAll(5);
	TestEqual(TEXT("Invisible mode keeps PrototypeState as Disturbing"), MallGhostAI::ReadString(Ghost, TEXT("PrototypeState")), FString(TEXT("Disturbing")));

	// 2. Switching to VisibleHunt at HomeLocation with no player transitions StateTree to Idle
	Interaction->SetGhostMode(EMallGhostMode::VisibleHunt);
	Worlds.TickAll(5);
	TestEqual(TEXT("VisibleHunt at HomeLocation with no player enters Idle state"), MallGhostAI::ReadString(Ghost, TEXT("PrototypeState")), FString(TEXT("Idle")));

	// 3. Setting CooldownUntil in the future transitions StateTree to Cooldown
	MallGhostAI::WriteDouble(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 5.0);
	Worlds.TickAll(5);
	TestEqual(TEXT("Active cooldown transitions StateTree to Cooldown state"), MallGhostAI::ReadString(Ghost, TEXT("PrototypeState")), FString(TEXT("Cooldown")));

	// 4. Clearing CooldownUntil transitions StateTree back to Idle
	MallGhostAI::WriteDouble(Ghost, TEXT("CooldownUntil"), 0.0);
	Worlds.TickAll(5);
	TestEqual(TEXT("Expired cooldown transitions StateTree back to Idle state"), MallGhostAI::ReadString(Ghost, TEXT("PrototypeState")), FString(TEXT("Idle")));

	return true;
}
#endif
