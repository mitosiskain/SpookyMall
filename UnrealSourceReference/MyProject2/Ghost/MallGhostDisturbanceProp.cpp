#include "Ghost/MallGhostDisturbanceProp.h"

#include "Ghost/MallGhostInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMallGhostDisturbanceProp::AMallGhostDisturbanceProp()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(5.0f);
	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	SetRootComponent(PropMesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PropMesh->SetStaticMesh(Cube.Object);
	PropMesh->SetRelativeScale3D(FVector(0.12, 0.12, 0.18));
	PropMesh->SetMobility(EComponentMobility::Movable);
	PropMesh->SetSimulatePhysics(true);
	PropMesh->SetEnableGravity(true);
	PropMesh->SetLinearDamping(1.2f);
	PropMesh->SetAngularDamping(2.0f);
	PropMesh->SetCanEverAffectNavigation(false);
	EnforceHarmlessPhysics();
}

void AMallGhostDisturbanceProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallGhostDisturbanceProp, DisturbanceCount);
	DOREPLIFETIME(AMallGhostDisturbanceProp, bLastDisturbanceWasThrow);
	DOREPLIFETIME(AMallGhostDisturbanceProp, BoundaryRecoveryCount);
}

void AMallGhostDisturbanceProp::BeginPlay()
{
	Super::BeginPlay();
	EnforceHarmlessPhysics();
	PropMesh->SetPhysicsMaxAngularVelocityInDegrees(120.0f);
}

void AMallGhostDisturbanceProp::EnforceHarmlessPhysics()
{
	PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PropMesh->SetCollisionObjectType(ECC_PhysicsBody);
	PropMesh->SetCollisionResponseToAllChannels(ECR_Block);
	// Ignore the entire player capsule, even during a throw. There is no damage callback.
	PropMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PropMesh->SetGenerateOverlapEvents(false);
	PropMesh->SetNotifyRigidBodyCollision(false);
	PropMesh->SetCanEverAffectNavigation(false);
}

bool AMallGhostDisturbanceProp::CanBeDisturbedBy(AActor* Ghost) const
{
	if (!HasAuthority() || !IsValid(Ghost) || Ghost->GetWorld() != GetWorld() || !PropMesh || !PropMesh->IsSimulatingPhysics())
	{
		return false;
	}
	const UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	return Interaction && Interaction->CanPerformDisturbance()
		&& GetWorld()->GetTimeSeconds() >= NextDisturbanceTime
		&& UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, this);
}

bool AMallGhostDisturbanceProp::Disturb(AActor* Ghost, bool bThrow)
{
	if (!CanBeDisturbedBy(Ghost))
	{
		return false;
	}
	if (!bHasSafeHome)
	{
		SafeHomeTransform = GetActorTransform();
		bHasSafeHome = true;
	}
	DisturbingGhost = Ghost;
	EnforceHarmlessPhysics();
	const float Side = (DisturbanceCount % 2 == 0) ? 1.0f : -1.0f;
	const FVector NewVelocity = (bThrow ? FVector(105.0f * Side, 65.0f, 145.0f) : FVector(40.0f * Side, -25.0f, 18.0f))
		.GetClampedToMaxSize(FMath::Clamp(MaximumSpeedCmPerSecond, 20.0f, 300.0f));
	// Set a bounded velocity rather than accumulating impulses across repeated requests.
	PropMesh->SetPhysicsLinearVelocity(NewVelocity, false);
	PropMesh->SetPhysicsAngularVelocityInDegrees(FVector(30.0f, 45.0f * Side, 20.0f), false);
	PropMesh->WakeAllRigidBodies();
	++DisturbanceCount;
	bLastDisturbanceWasThrow = bThrow;
	NextDisturbanceTime = GetWorld()->GetTimeSeconds() + 2.0;
	ForceNetUpdate();
	return true;
}

void AMallGhostDisturbanceProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bHasSafeHome)
	{
		return;
	}
	AActor* Ghost = DisturbingGhost.Get();
	if (IsValid(Ghost) && !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, this))
	{
		RecoverToHome();
		return;
	}
	const FVector Velocity = PropMesh->GetPhysicsLinearVelocity();
	const float Limit = FMath::Clamp(MaximumSpeedCmPerSecond, 20.0f, 300.0f);
	if (Velocity.ContainsNaN() || GetActorLocation().ContainsNaN())
	{
		RecoverToHome();
	}
	else if (Velocity.SizeSquared() > FMath::Square(Limit))
	{
		PropMesh->SetPhysicsLinearVelocity(Velocity.GetClampedToMaxSize(Limit));
	}
}

void AMallGhostDisturbanceProp::RecoverToHome()
{
	PropMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	PropMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	SetActorTransform(SafeHomeTransform, false, nullptr, ETeleportType::ResetPhysics);
	++BoundaryRecoveryCount;
	ForceNetUpdate();
}
