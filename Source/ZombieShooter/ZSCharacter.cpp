// Fill out your copyright notice in the Description page of Project Settings.


#include "ZSCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "ZSCharacterMovement.h"

AZSCharacter::AZSCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UZSCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Enable replication of properties
	bReplicates = true;

	// Set up components
	Camera1P = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("CameraComp1P"));
	Camera1P->SetupAttachment(GetCapsuleComponent());

	Mesh1P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("PawnMesh1P"));
	Mesh1P->SetupAttachment(Camera1P);
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bOwnerNoSee = false;
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh1P->PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Mesh1P->SetCollisionObjectType(ECC_Pawn);
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetCollisionResponseToAllChannels(ECR_Ignore);

	GetMesh()->bOnlyOwnerSee = false;
	GetMesh()->bOwnerNoSee = true;
	GetMesh()->bReceivesDecals = false;
	GetMesh()->SetCollisionObjectType(ECC_Pawn);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	//GetMesh()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
	//GetMesh()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Set defaults
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	RunningSpeedModifier = 1.5f;
	bWantsToRun = false;
	bWantsToRunToggled = false;
}

void AZSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Ends running toggle if character is no longer running
	if (bWantsToRunToggled && !IsRunning())
	{
		SetRunning(false, false);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AZSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AZSCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AZSCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AZSCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AZSCharacter::LookUpAtRate);

	//PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AZSCharacter::OnStartFire);
	//PlayerInputComponent->BindAction("Fire", IE_Released, this, &AZSCharacter::OnStopFire);

	//PlayerInputComponent->BindAction("Targeting", IE_Pressed, this, &AZSCharacter::OnStartTargeting);
	//PlayerInputComponent->BindAction("Targeting", IE_Released, this, &AZSCharacter::OnStopTargeting);

	//PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AZSCharacter::OnReload);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AZSCharacter::OnStartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AZSCharacter::OnStopJump);

	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &AZSCharacter::OnStartRunning);
	PlayerInputComponent->BindAction("RunToggle", IE_Pressed, this, &AZSCharacter::OnStartRunningToggle);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &AZSCharacter::OnStopRunning);
}

void AZSCharacter::MoveForward(float Val)
{
	if (Val != 0.f)
	{
		const FVector Direction = GetActorForwardVector();
		AddMovementInput(Direction, Val);
	}
}


void AZSCharacter::MoveRight(float Val)
{
	if (Val != 0.f)
	{
		const FVector Direction = GetActorRightVector();
		AddMovementInput(Direction, Val);
	}
}

void AZSCharacter::TurnAtRate(float Val)
{
	// calculate rotation delta for this frame from the rate information
	AddControllerYawInput(Val * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AZSCharacter::LookUpAtRate(float Val)
{
	// calculate rotation delta for this frame from the rate information
	AddControllerPitchInput(Val * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AZSCharacter::OnStartJump()
{
	// check if player controller allows game input *
	bPressedJump = true;
}

void AZSCharacter::OnStopJump()
{
	bPressedJump = false;
	StopJumping();
}

void AZSCharacter::OnStartRunning()
{
	// check if targeting *
	// also stop weapon fire *
	SetRunning(true, false);
}

void AZSCharacter::OnStartRunningToggle()
{
	// check if targeting *
	// also stop weapon fire *
	SetRunning(true, true); // See Tick function for how toggled running is stopped
}

void AZSCharacter::OnStopRunning()
{
	SetRunning(false, false);
}

bool AZSCharacter::IsRunning() const
{
	if (!GetCharacterMovement())
	{
		return false;
	}

	return (bWantsToRun || bWantsToRunToggled) && !GetVelocity().IsZero() && 
		(GetVelocity().GetSafeNormal2D() | GetActorForwardVector()) > -0.1; // Last expression evaluates whether or not character is moving backward
}

//////////////////////////////////////////////////////////////////////////
// Movement

void AZSCharacter::SetRunning(bool bNewRunning, bool bToggle)
{
	bWantsToRun = bNewRunning;
	bWantsToRunToggled = bNewRunning && bToggle;

	if (GetLocalRole() < ROLE_Authority) // either ROLE_NONE, SimulatedProxy, or AutonomousProxy
	{
		// Passes the run message to the server if this actor is on a client machine
		ServerSetRunning(bNewRunning, bToggle);
	}
}

//////////////////////////////////////////////////////////////////////////
// Server side functions

bool AZSCharacter::ServerSetRunning_Validate(bool bNewRunning, bool bToggle)
{
	return true;
}

void AZSCharacter::ServerSetRunning_Implementation(bool bNewRunning, bool bToggle)
{
	SetRunning(bNewRunning, bToggle);
}

//////////////////////////////////////////////////////////////////////////
// Replication

void AZSCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only to local owner: weapon change requests are locally instigated, other clients don't need it
	//DOREPLIFETIME_CONDITION(AShooterCharacter, Inventory, COND_OwnerOnly);

	// everyone except local owner: flag change is locally instigated
	//DOREPLIFETIME_CONDITION(AShooterCharacter, bIsTargeting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AZSCharacter, bWantsToRun, COND_SkipOwner);

	//DOREPLIFETIME_CONDITION(AShooterCharacter, LastTakeHitInfo, COND_Custom);

	// everyone
	//DOREPLIFETIME(AShooterCharacter, CurrentWeapon);
	//DOREPLIFETIME(AShooterCharacter, Health);
}

//////////////////////////////////////////////////////////////////////////
// Reading data

float AZSCharacter::GetRunningSpeedModifier() const
{
	return RunningSpeedModifier;
}