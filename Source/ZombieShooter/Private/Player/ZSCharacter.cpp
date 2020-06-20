// Fill out your copyright notice in the Description page of Project Settings.


#include "ZombieShooter/Public/Player/ZSCharacter.h"
//#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "ZombieShooter/Public/Player/ZSCharacterMovement.h"
#include "ZombieShooter/Public/Weapons/ZSWeapon.h"
#include "ZombieShooter/Public/Player/ZSPlayerController.h"
#include "ZSMacros.h"

// Initialize global static notifications
FOnShooterCharacterEquipWeapon AZSCharacter::NotifyEquipWeapon;
FOnShooterCharacterUnEquipWeapon AZSCharacter::NotifyUnEquipWeapon;

AZSCharacter::AZSCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UZSCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Enable replication of properties
	bReplicates = true;

	// Set up components
	//Camera1P = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("CameraComp1P"));
	//Camera1P->SetupAttachment(GetCapsuleComponent());

	Mesh1P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("PawnMesh1P"));
	//Mesh1P->SetupAttachment(Camera1P);
	Mesh1P->SetupAttachment(GetCapsuleComponent());
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
	GetMesh()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
	
	// Set defaults
	TargetingSpeedModifier = 0.5f;
	bIsTargeting = false;
	RunningSpeedModifier = 1.5f;
	bWantsToRun = false;
	bWantsToFire = false;
	//~ LowHealthPercentage = 0.5f;

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
}

void AZSCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetLocalRole() == ROLE_Authority)
	{
		//~ Health = GetMaxHealth();

		// Needs to happen after character is added to repgraph
		GetWorldTimerManager().SetTimerForNextTick(this, &AZSCharacter::SpawnDefaultInventory);
	}

	// set initial mesh visibility (3rd person view)
	//~UpdatePawnMeshes();

	/*~
	// create material instance for setting team colors (3rd person view)
	for (int32 iMat = 0; iMat < GetMesh()->GetNumMaterials(); iMat++)
	{
		MeshMIDs.Add(GetMesh()->CreateAndSetMaterialInstanceDynamic(iMat));
	}
	*/

	/*~
	// play respawn effects
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (RespawnFX)
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, RespawnFX, GetActorLocation(), GetActorRotation());
		}

		if (RespawnSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, RespawnSound, GetActorLocation());
		}
	}
	*/
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
// Meshes

void AZSCharacter::UpdatePawnMeshes()
{
	bool const bFirstPerson = IsFirstPerson();

	Mesh1P->VisibilityBasedAnimTickOption = !bFirstPerson ? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered : EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh1P->SetOwnerNoSee(!bFirstPerson);

	GetMesh()->VisibilityBasedAnimTickOption = bFirstPerson ? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered : EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetOwnerNoSee(bFirstPerson);
}

void AZSCharacter::OnCameraUpdate(const FVector& CameraLocation, const FRotator& CameraRotation)
{
	USkeletalMeshComponent* DefMesh1P = Cast<USkeletalMeshComponent>(GetClass()->GetDefaultSubobjectByName(TEXT("PawnMesh1P")));
	const FMatrix DefMeshLS = FRotationTranslationMatrix(DefMesh1P->GetRelativeRotation(), DefMesh1P->GetRelativeLocation());
	const FMatrix LocalToWorld = ActorToWorld().ToMatrixWithScale();

	// Mesh rotating code expect uniform scale in LocalToWorld matrix

	const FRotator RotCameraPitch(CameraRotation.Pitch, 0.0f, 0.0f);
	const FRotator RotCameraYaw(0.0f, CameraRotation.Yaw, 0.0f);

	const FMatrix LeveledCameraLS = FRotationTranslationMatrix(RotCameraYaw, CameraLocation) * LocalToWorld.Inverse();
	const FMatrix PitchedCameraLS = FRotationMatrix(RotCameraPitch) * LeveledCameraLS;
	const FMatrix MeshRelativeToCamera = DefMeshLS * LeveledCameraLS.Inverse();
	const FMatrix PitchedMesh = MeshRelativeToCamera * PitchedCameraLS;

	Mesh1P->SetRelativeLocationAndRotation(PitchedMesh.GetOrigin(), PitchedMesh.Rotator());
}


//////////////////////////////////////////////////////////////////////////
// Inventory

void AZSCharacter::SpawnDefaultInventory()
{
	// do not spawn inventory on client side
	if (GetLocalRole() < ROLE_Authority)
	{
		return;
	}

	int32 NumWeaponClasses = DefaultInventoryClasses.Num();
	for (int32 i = 0; i < NumWeaponClasses; i++)
	{
		if (DefaultInventoryClasses[i])
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AZSWeapon* NewWeapon = GetWorld()->SpawnActor<AZSWeapon>(DefaultInventoryClasses[i], SpawnInfo);
			AddWeapon(NewWeapon);
		}
	}

	// equip first weapon in inventory
	if (Inventory.Num() > 0)
	{
		EquipWeapon(Inventory[0]);
	}
}

void AZSCharacter::DestroyInventory()
{
	// do not destroy inventory on client side
	if (GetLocalRole() < ROLE_Authority)
	{
		return;
	}

	// remove all weapons from inventory and destroy them
	for (int32 i = Inventory.Num() - 1; i >= 0; i--)
	{
		AZSWeapon* Weapon = Inventory[i];
		if (Weapon)
		{
			RemoveWeapon(Weapon);
			Weapon->Destroy();
		}
	}
}

void AZSCharacter::AddWeapon(AZSWeapon* Weapon)
{
	if (Weapon && GetLocalRole() == ROLE_Authority)
	{
		Weapon->OnEnterInventory(this);
		Inventory.AddUnique(Weapon);
	}
}

void AZSCharacter::RemoveWeapon(AZSWeapon* Weapon)
{
	if (Weapon && GetLocalRole() == ROLE_Authority)
	{
		Weapon->OnLeaveInventory();
		Inventory.RemoveSingle(Weapon);
	}
}

AZSWeapon* AZSCharacter::FindWeapon(TSubclassOf<AZSWeapon> WeaponClass)
{
	for (int32 i = 0; i < Inventory.Num(); i++)
	{
		if (Inventory[i] && Inventory[i]->IsA(WeaponClass))
		{
			return Inventory[i];
		}
	}

	return nullptr;
}

void AZSCharacter::EquipWeapon(AZSWeapon* Weapon)
{
	if (Weapon)
	{
		if (GetLocalRole() == ROLE_Authority)
		{
			SetCurrentWeapon(Weapon, CurrentWeapon);
		}
		else
		{
			ServerEquipWeapon(Weapon);
		}
	}
}

bool AZSCharacter::ServerEquipWeapon_Validate(AZSWeapon* Weapon)
{
	return true;
}

void AZSCharacter::ServerEquipWeapon_Implementation(AZSWeapon* Weapon)
{
	EquipWeapon(Weapon);
}

void AZSCharacter::OnRep_CurrentWeapon(AZSWeapon* LastWeapon)
{
	SetCurrentWeapon(CurrentWeapon, LastWeapon);
}

void AZSCharacter::SetCurrentWeapon(AZSWeapon* NewWeapon, AZSWeapon* LastWeapon)
{
	AZSWeapon* LocalLastWeapon = nullptr;

	if (LastWeapon != nullptr)
	{
		LocalLastWeapon = LastWeapon;
	}
	else if (NewWeapon != CurrentWeapon)
	{
		LocalLastWeapon = CurrentWeapon;
	}

	// unequip previous
	if (LocalLastWeapon)
	{
		LocalLastWeapon->OnUnEquip();
	}

	CurrentWeapon = NewWeapon;

	// equip new one
	if (NewWeapon)
	{
		NewWeapon->SetOwningPawn(this);	// Make sure weapon's MyPawn is pointing back to us. During replication, we can't guarantee APawn::CurrentWeapon will rep after AWeapon::MyPawn!

		NewWeapon->OnEquip(LastWeapon);
	}
}

//////////////////////////////////////////////////////////////////////////
// Weapon usage

void AZSCharacter::StartWeaponFire()
{
	if (!bWantsToFire)
	{
		bWantsToFire = true;
		if (CurrentWeapon)
		{
			CurrentWeapon->StartFire();
		}
	}
}

void AZSCharacter::StopWeaponFire()
{
	if (bWantsToFire)
	{
		bWantsToFire = false;
		if (CurrentWeapon)
		{
			CurrentWeapon->StopFire();
		}
	}
}

bool AZSCharacter::CanFire() const
{
	return IsAlive();
}

bool AZSCharacter::CanReload() const
{
	return true;
}

void AZSCharacter::SetTargeting(bool bNewTargeting)
{
	bIsTargeting = bNewTargeting;

	/*~
	if (TargetingSound)
	{
		UGameplayStatics::SpawnSoundAttached(TargetingSound, GetRootComponent());
	}
	*/

	// Call SetTargeting on server if on client side
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerSetTargeting(bNewTargeting);
	}
}

bool AZSCharacter::ServerSetTargeting_Validate(bool bNewTargeting)
{
	return true;
}

void AZSCharacter::ServerSetTargeting_Implementation(bool bNewTargeting)
{
	SetTargeting(bNewTargeting);
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
// Input handlers

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

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AZSCharacter::OnStartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AZSCharacter::OnStopFire);

	PlayerInputComponent->BindAction("Targeting", IE_Pressed, this, &AZSCharacter::OnStartTargeting);
	PlayerInputComponent->BindAction("Targeting", IE_Released, this, &AZSCharacter::OnStopTargeting);

	PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &AZSCharacter::OnNextWeapon);
	PlayerInputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AZSCharacter::OnPrevWeapon);

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AZSCharacter::OnReload);

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

void AZSCharacter::OnStartFire()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		// stop running when shooting
		if (IsRunning())
		{
			SetRunning(false, false);
		}
		StartWeaponFire();
	}
}

void AZSCharacter::OnStopFire()
{
	StopWeaponFire();
}

void AZSCharacter::OnStartTargeting()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		// stop running when targeting
		if (IsRunning())
		{
			SetRunning(false, false);
		}
		SetTargeting(true);
	}
}

void AZSCharacter::OnStopTargeting()
{
	SetTargeting(false);
}

void AZSCharacter::OnNextWeapon()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		// switch to next weapon if there is enough inventory and the player either does not have a current weapon or the current weapon is not currently being equipped
		if (Inventory.Num() >= 2 && (CurrentWeapon == nullptr || CurrentWeapon->GetCurrentState() != EWeaponState::Equipping))
		{
			const int32 CurrentWeaponIdx = Inventory.IndexOfByKey(CurrentWeapon);
			AZSWeapon* NextWeapon = Inventory[(CurrentWeaponIdx + 1) % Inventory.Num()];
			EquipWeapon(NextWeapon);
		}
	}
}

void AZSCharacter::OnPrevWeapon()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		// switch to next weapon if there is enough inventory and the player either does not have a current weapon or the current weapon is not currently being equipped
		if (Inventory.Num() >= 2 && (CurrentWeapon == nullptr || CurrentWeapon->GetCurrentState() != EWeaponState::Equipping))
		{
			const int32 CurrentWeaponIdx = Inventory.IndexOfByKey(CurrentWeapon);
			AZSWeapon* PrevWeapon = Inventory[(CurrentWeaponIdx - 1 + Inventory.Num()) % Inventory.Num()];
			EquipWeapon(PrevWeapon);
		}
	}
}

void AZSCharacter::OnReload()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (CurrentWeapon)
		{
			CurrentWeapon->StartReload();
		}
	}
}

void AZSCharacter::OnStartRunning()
{
	AZSPlayerController*  MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (IsTargeting())
		{
			SetTargeting(false);
		}
		StopWeaponFire();
		SetRunning(true, false);
	}
}

void AZSCharacter::OnStartRunningToggle()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		if (IsTargeting())
		{
			SetTargeting(false);
		}
		StopWeaponFire();
		SetRunning(true, true); // See Tick function for how toggled running is stopped
	}
}

void AZSCharacter::OnStopRunning()
{
	SetRunning(false, false);
}

void AZSCharacter::OnStartJump()
{
	AZSPlayerController* MyPC = Cast<AZSPlayerController>(Controller);
	if (MyPC && MyPC->IsGameInputAllowed())
	{
		bPressedJump = true;
	}
}

void AZSCharacter::OnStopJump()
{
	bPressedJump = false;
	StopJumping();
}

//////////////////////////////////////////////////////////////////////////
// Replication

void AZSCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only to local owner: weapon change requests are locally instigated, other clients don't need it
	DOREPLIFETIME_CONDITION(AZSCharacter, Inventory, COND_OwnerOnly);

	// everyone except local owner: flag change is locally instigated
	//DOREPLIFETIME_CONDITION(AShooterCharacter, bIsTargeting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AZSCharacter, bWantsToRun, COND_SkipOwner);

	//~DOREPLIFETIME_CONDITION(AZSCharacter, LastTakeHitInfo, COND_Custom);

	// everyone
	DOREPLIFETIME(AZSCharacter, CurrentWeapon);
	//~ DOREPLIFETIME(AShooterCharacter, Health);
}

//////////////////////////////////////////////////////////////////////////
// Reading data

USkeletalMeshComponent* AZSCharacter::GetPawnMesh() const
{
	return IsFirstPerson() ? Mesh1P : GetMesh();
}

AZSWeapon* AZSCharacter::GetWeapon() const
{
	return CurrentWeapon;
}

int32 AZSCharacter::GetInventoryCount() const
{
	return Inventory.Num();
}

AZSWeapon* AZSCharacter::GetInventoryWeapon(int32 index) const
{
	return Inventory[index];
}

FName AZSCharacter::GetWeaponAttachPoint() const
{
	return WeaponAttachPoint;
}

float AZSCharacter::GetTargetingSpeedModifier() const
{
	return TargetingSpeedModifier;
}

bool AZSCharacter::IsTargeting() const
{
	return bIsTargeting;
}

bool AZSCharacter::IsFiring() const
{
	return bWantsToFire;
};

float AZSCharacter::GetRunningSpeedModifier() const
{
	return RunningSpeedModifier;
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

bool AZSCharacter::IsFirstPerson() const
{
	return IsAlive() && Controller && Controller->IsLocalPlayerController();
}

bool AZSCharacter::IsAlive() const
{
	return true; //~ Health > 0;
}

USkeletalMeshComponent* AZSCharacter::GetSpecificPawnMesh(bool WantFirstPerson) const
{
	return WantFirstPerson == true ? Mesh1P : GetMesh();
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

FRotator AZSCharacter::GetAimOffsets() const
{
	const FVector AimDirWS = GetBaseAimRotation().Vector();
	const FVector AimDirLS = ActorToWorld().InverseTransformVectorNoScale(AimDirWS);
	const FRotator AimRotLS = AimDirLS.Rotation();

	return AimRotLS;
}