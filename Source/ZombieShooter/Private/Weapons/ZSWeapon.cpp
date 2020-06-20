// Fill out your copyright notice in the Description page of Project Settings.


#include "ZombieShooter/Public/Weapons/ZSWeapon.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "ZombieShooter/Public/Player/ZSCharacter.h"
#include "ZombieShooter/Public/Player/ZSPlayerController.h"
#include "ZSMacros.h"

AZSWeapon::AZSWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set up components
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh1P"));
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh1P->bReceivesDecals = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetCollisionResponseToAllChannels(ECR_Ignore);
	RootComponent = Mesh1P;

	Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh3P"));
	Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh3P->bReceivesDecals = false;
	Mesh3P->CastShadow = true;
	Mesh3P->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh3P->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh3P->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
	Mesh3P->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh3P->SetCollisionResponseToChannel(COLLISION_PROJECTILE, ECR_Block);
	Mesh3P->SetupAttachment(Mesh1P);

	// Set defaults
	bLoopedMuzzleFX = false;
	bLoopedFireAnim = false;
	bPlayingFireAnim = false;
	bIsEquipped = false;
	bWantsToFire = false;
	bPendingReload = false;
	bPendingEquip = false;
	CurrentState = EWeaponState::Idle;

	CurrentAmmo = 0;
	CurrentAmmoInClip = 0;
	BurstCounter = 0;
	LastFireTime = 0.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	//~ SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
}

void AZSWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (WeaponConfig.InitialClips > 0)
	{
		CurrentAmmoInClip = WeaponConfig.AmmoPerClip;
		CurrentAmmo = WeaponConfig.AmmoPerClip * WeaponConfig.InitialClips;
	}

	DetachMeshFromPawn();
}

void AZSWeapon::Destroyed()
{
	Super::Destroyed();

	StopSimulatingWeaponFire();
}


//////////////////////////////////////////////////////////////////////////
// Inventory

void AZSWeapon::OnEquip(const AZSWeapon* LastWeapon)
{
	AttachMeshToPawn();

	bPendingEquip = true;
	DetermineWeaponState();

	// Only play animation if last weapon is valid
	if (LastWeapon)
	{
		float Duration = PlayWeaponAnimation(EquipAnim);
		if (Duration <= 0.0f)
		{
			// failsafe
			Duration = 0.5f;
		}
		EquipStartedTime = GetWorld()->GetTimeSeconds();
		EquipDuration = Duration;

		GetWorldTimerManager().SetTimer(TimerHandle_OnEquipFinished, this, &AZSWeapon::OnEquipFinished, Duration, false);
	}
	else
	{
		OnEquipFinished();
	}

	if (MyPawn && MyPawn->IsLocallyControlled())
	{
		PlayWeaponSound(EquipSound);
	}

	AZSCharacter::NotifyEquipWeapon.Broadcast(MyPawn, this);
}

void AZSWeapon::OnEquipFinished()
{
	AttachMeshToPawn();

	bIsEquipped = true;
	bPendingEquip = false;

	// Determine the state so that the can reload checks will work
	DetermineWeaponState();

	if (MyPawn)
	{
		// try to reload empty clip
		if (MyPawn->IsLocallyControlled() &&
			CurrentAmmoInClip <= 0 &&
			CanReload())
		{
			StartReload();
		}
	}
}

void AZSWeapon::OnUnEquip()
{
	DetachMeshFromPawn();
	bIsEquipped = false;
	StopFire();

	if (bPendingReload)
	{
		StopWeaponAnimation(ReloadAnim);
		bPendingReload = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
		GetWorldTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
	}

	if (bPendingEquip)
	{
		StopWeaponAnimation(EquipAnim);
		bPendingEquip = false;

		GetWorldTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
	}

	AZSCharacter::NotifyUnEquipWeapon.Broadcast(MyPawn, this);

	DetermineWeaponState();
}

void AZSWeapon::OnEnterInventory(AZSCharacter* NewOwner)
{
	SetOwningPawn(NewOwner);
}

void AZSWeapon::OnLeaveInventory()
{
	if (IsAttachedToPawn())
	{
		OnUnEquip();
	}

	if (GetLocalRole() == ROLE_Authority)
	{
		SetOwningPawn(nullptr);
	}
}

void AZSWeapon::AttachMeshToPawn()
{
	if (MyPawn)
	{
		// Remove and hide both first and third person meshes
		DetachMeshFromPawn();
		
		// For locally controller players we attach both weapons and let the bOnlyOwnerSee, bOwnerNoSee flags deal with visibility.
		FName AttachPoint = MyPawn->GetWeaponAttachPoint();
		if (MyPawn->IsLocallyControlled())
		{
			USkeletalMeshComponent* PawnMesh1p = MyPawn->GetSpecificPawnMesh(true);
			USkeletalMeshComponent* PawnMesh3p = MyPawn->GetSpecificPawnMesh(false);
			Mesh1P->SetHiddenInGame(false);
			Mesh3P->SetHiddenInGame(false);
			Mesh1P->AttachToComponent(PawnMesh1p, FAttachmentTransformRules::KeepRelativeTransform, AttachPoint);
			Mesh3P->AttachToComponent(PawnMesh3p, FAttachmentTransformRules::KeepRelativeTransform, AttachPoint);
		}
		else
		{
			USkeletalMeshComponent* UseWeaponMesh = GetWeaponMesh();
			USkeletalMeshComponent* UsePawnMesh = MyPawn->GetPawnMesh();
			UseWeaponMesh->AttachToComponent(UsePawnMesh, FAttachmentTransformRules::KeepRelativeTransform, AttachPoint);
			UseWeaponMesh->SetHiddenInGame(false);
		}
	}
}

void AZSWeapon::DetachMeshFromPawn()
{
	Mesh1P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Mesh1P->SetHiddenInGame(true);

	Mesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Mesh3P->SetHiddenInGame(true);
}


//////////////////////////////////////////////////////////////////////////
// Input

void AZSWeapon::StartFire()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerStartFire();
	}

	if (!bWantsToFire)
	{
		bWantsToFire = true;
		DetermineWeaponState();
	}
}

void AZSWeapon::StopFire()
{
	if ((GetLocalRole() < ROLE_Authority) && MyPawn && MyPawn->IsLocallyControlled())
	{
		ServerStopFire();
	}

	if (bWantsToFire)
	{
		bWantsToFire = false;
		DetermineWeaponState();
	}
}

void AZSWeapon::StartReload(bool bFromReplication)
{
	if (!bFromReplication && GetLocalRole() < ROLE_Authority)
	{
		ServerStartReload();
	}

	if (bFromReplication || CanReload())
	{
		bPendingReload = true;
		DetermineWeaponState();

		float AnimDuration = PlayWeaponAnimation(ReloadAnim);
		if (AnimDuration <= 0.f)
		{
			// failsafe in case animation does not exist
			AnimDuration = WeaponConfig.NoAnimReloadDuration;
		}

		GetWorldTimerManager().SetTimer(TimerHandle_StopReload, this, &AZSWeapon::StopReload, AnimDuration, false);
		if (GetLocalRole() == ROLE_Authority)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_ReloadWeapon, this, &AZSWeapon::ReloadWeapon, FMath::Max(0.1f, AnimDuration - 0.1f), false);
		}

		if (MyPawn && MyPawn->IsLocallyControlled())
		{
			PlayWeaponSound(ReloadSound);
		}
	}
}

void AZSWeapon::StopReload()
{
	if (CurrentState == EWeaponState::Reloading)
	{
		bPendingReload = false;
		DetermineWeaponState();
		StopWeaponAnimation(ReloadAnim);
	}
}

bool AZSWeapon::ServerStartFire_Validate()
{
	return true;
}

void AZSWeapon::ServerStartFire_Implementation()
{
	StartFire();
}

bool AZSWeapon::ServerStopFire_Validate()
{
	return true;
}

void AZSWeapon::ServerStopFire_Implementation()
{
	StopFire();
}

bool AZSWeapon::ServerStartReload_Validate()
{
	return true;
}

void AZSWeapon::ServerStartReload_Implementation()
{
	StartReload();
}

bool AZSWeapon::ServerStopReload_Validate()
{
	return true;
}

void AZSWeapon::ServerStopReload_Implementation()
{
	StopReload();
}

void AZSWeapon::ClientStartReload_Implementation()
{
	StartReload();
}


//////////////////////////////////////////////////////////////////////////
// Control

bool AZSWeapon::CanFire() const
{
	const bool bCanFire = MyPawn && MyPawn->CanFire();
	const bool bStateOKToFire = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
	return ((bCanFire == true) && (bStateOKToFire == true) && (bPendingReload == false));
}

bool AZSWeapon::CanReload() const
{
	const bool bCanReload = (!MyPawn || MyPawn->CanReload());
	bool bGotAmmo = (CurrentAmmoInClip < WeaponConfig.AmmoPerClip) && (CurrentAmmo - CurrentAmmoInClip > 0 || HasInfiniteClip()); //~ perhaps infinite clip is a typo for infinite ammo
	bool bStateOKToReload = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
	return ((bCanReload == true) && (bGotAmmo == true) && (bStateOKToReload == true));
}


//////////////////////////////////////////////////////////////////////////
// Ammo

void AZSWeapon::GiveAmmo(int AddAmount)
{
	const int32 MissingAmmo = FMath::Max(0, WeaponConfig.MaxAmmo - CurrentAmmo);
	AddAmount = FMath::Min(AddAmount, MissingAmmo); // do not add more than necessary to get to max ammo
	CurrentAmmo += AddAmount;

	/*~
	AShooterAIController* BotAI = MyPawn ? Cast<AShooterAIController>(MyPawn->GetController()) : NULL;
	if (BotAI)
	{
		BotAI->CheckAmmo(this);
	}
	*/

	// start reload if clip was empty
	if (GetCurrentAmmoInClip() <= 0 &&
		CanReload() &&
		MyPawn && (MyPawn->GetWeapon() == this)) // do not reload if this is not pawn's current weapon
	{
		ClientStartReload();
	}
}

void AZSWeapon::UseAmmo()
{
	if (!HasInfiniteAmmo())
	{
		CurrentAmmoInClip--;

		if (!HasInfiniteClip())
		{
			CurrentAmmo--;
		}
	}

	//~ AShooterAIController* BotAI = MyPawn ? Cast<AShooterAIController>(MyPawn->GetController()) : NULL;
	AZSPlayerController* PC = MyPawn ? Cast<AZSPlayerController>(MyPawn->GetController()) : nullptr;
	/*~if (BotAI)
	{
		BotAI->CheckAmmo(this);
	}
	else*/ /*~ if (PC)
	{
		AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerController->PlayerState);
		switch (GetAmmoType())
		{
		case EAmmoType::ERocket:
			PlayerState->AddRocketsFired(1);
			break;
		case EAmmoType::EBullet:
		default:
			PlayerState->AddBulletsFired(1);
			break;
		}
	} */
}


//////////////////////////////////////////////////////////////////////////
// Weapon usage

void AZSWeapon::HandleReFiring()
{
	// Update TimerIntervalAdjustment
	UWorld* MyWorld = GetWorld();

	float SlackTimeThisFrame = FMath::Max(0.0f, (MyWorld->TimeSeconds - LastFireTime) - WeaponConfig.TimeBetweenShots);

	if (bAllowAutomaticWeaponCatchup)
	{
		TimerIntervalAdjustment -= SlackTimeThisFrame;
	}

	HandleFiring();
}

void AZSWeapon::HandleFiring()
{
	if ((CurrentAmmoInClip > 0 || HasInfiniteClip() || HasInfiniteAmmo()) && CanFire())
	{
		// see pre-conditions for SimulateWeaponFire()
		if (GetNetMode() != NM_DedicatedServer)
		{
			SimulateWeaponFire();
		}

		if (MyPawn && MyPawn->IsLocallyControlled())
		{
			FireWeapon();

			UseAmmo();

			// update firing FX on remote clients if function was called on server
			BurstCounter++;
		}
	}
	else if (CanReload())
	{
		StartReload();
	}
	else if (MyPawn && MyPawn->IsLocallyControlled())
	{
		if (GetCurrentAmmo() == 0 && !bRefiring)
		{
			PlayWeaponSound(OutOfAmmoSound);
			AZSPlayerController* MyPC = Cast<AZSPlayerController>(MyPawn->Controller);
			/*~AShooterHUD* MyHUD = MyPC ? Cast<AShooterHUD>(MyPC->GetHUD()) : NULL;
			if (MyHUD)
			{
				MyHUD->NotifyOutOfAmmo();
			}*/
		}

		// stop weapon fire FX, but stay in Firing state
		if (BurstCounter > 0)
		{
			OnBurstFinished();
		}
	}
	else
	{
		OnBurstFinished();
	}

	if (MyPawn && MyPawn->IsLocallyControlled())
	{
		// local client will notify server
		if (GetLocalRole() < ROLE_Authority)
		{
			ServerHandleFiring();
		}

		// reload after firing last round
		if (CurrentAmmoInClip <= 0 && CanReload())
		{
			StartReload();
		}

		// setup refire timer
		bRefiring = (CurrentState == EWeaponState::Firing && WeaponConfig.TimeBetweenShots > 0.0f);
		if (bRefiring)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AZSWeapon::HandleReFiring, FMath::Max<float>(WeaponConfig.TimeBetweenShots + TimerIntervalAdjustment, SMALL_NUMBER), false);
			TimerIntervalAdjustment = 0.f;
		}
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
}

bool AZSWeapon::ServerHandleFiring_Validate()
{
	return true;
}


void AZSWeapon::ServerHandleFiring_Implementation()
{
	const bool bShouldUpdateAmmo = (CurrentAmmoInClip > 0 && CanFire());

	HandleFiring();

	if (bShouldUpdateAmmo)
	{
		// update ammo
		UseAmmo();

		// update firing FX on remote clients
		BurstCounter++;
	}
}

// pre-condition: GetLocalRole() == ROLE_Authority
void AZSWeapon::ReloadWeapon()
{
	// do not add ammo if there is not enough
	// if there is not enough, clip delta = 0
	int32 ClipDelta = FMath::Min(WeaponConfig.AmmoPerClip - CurrentAmmoInClip, CurrentAmmo - CurrentAmmoInClip);

	if (HasInfiniteClip())
	{
		ClipDelta = WeaponConfig.AmmoPerClip - CurrentAmmoInClip;
	}

	if (ClipDelta > 0)
	{
		CurrentAmmoInClip += ClipDelta;
	}

	if (HasInfiniteClip())
	{
		CurrentAmmo = FMath::Max(CurrentAmmoInClip, CurrentAmmo);
	}
}

void AZSWeapon::SetWeaponState(EWeaponState NewState)
{
	const EWeaponState PrevState = CurrentState;
	if (PrevState == EWeaponState::Firing && NewState != EWeaponState::Firing)
	{
		OnBurstFinished();
	}

	CurrentState = NewState;

	if (PrevState != EWeaponState::Firing && NewState == EWeaponState::Firing)
	{
		OnBurstStarted();
	}
}

void AZSWeapon::DetermineWeaponState()
{
	EWeaponState NewState = EWeaponState::Idle;

	if (bIsEquipped)
	{
		if (bPendingReload)
		{
			if (CanReload() == false)
			{
				NewState = CurrentState;
			}
			else
			{
				NewState = EWeaponState::Reloading;
			}
		}
		else if (bWantsToFire == true && CanFire() == true)
		{
			NewState = EWeaponState::Firing;
		}
	}
	else if (bPendingEquip)
	{
		NewState = EWeaponState::Equipping;
	}

	SetWeaponState(NewState);
}

void AZSWeapon::OnBurstStarted()
{
	// start firing, can be delayed to satisfy TimeBetweenShots
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (LastFireTime > 0 && WeaponConfig.TimeBetweenShots > 0.0f &&
		LastFireTime + WeaponConfig.TimeBetweenShots > GameTime)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &AZSWeapon::HandleFiring, LastFireTime + WeaponConfig.TimeBetweenShots - GameTime, false);
	}
	else
	{
		HandleFiring();
	}
}

void AZSWeapon::OnBurstFinished()
{
	// stop firing FX on remote clients
	BurstCounter = 0;

	// stop firing FX locally, unless it's a dedicated server
	//if (GetNetMode() != NM_DedicatedServer)
	//{
	StopSimulatingWeaponFire();
	//}

	GetWorldTimerManager().ClearTimer(TimerHandle_HandleFiring);
	bRefiring = false;

	// reset firing interval adjustment
	TimerIntervalAdjustment = 0.0f;
}


//////////////////////////////////////////////////////////////////////////
// Weapon usage helpers

UAudioComponent* AZSWeapon::PlayWeaponSound(USoundCue* Sound)
{
	UAudioComponent* AC = nullptr;
	if (Sound && MyPawn)
	{
		AC = UGameplayStatics::SpawnSoundAttached(Sound, MyPawn->GetRootComponent());
	}

	return AC;
}

float AZSWeapon::PlayWeaponAnimation(const FWeaponAnim& Animation)
{
	float Duration = 0.f;
	if (MyPawn)
	{
		UAnimMontage* UseAnim = MyPawn->IsFirstPerson() ? Animation.Pawn1P : Animation.Pawn3P;
		if (UseAnim)
		{
			Duration = MyPawn->PlayAnimMontage(UseAnim);
		}
	}

	return Duration;
}

void AZSWeapon::StopWeaponAnimation(const FWeaponAnim& Animation)
{
	if (MyPawn)
	{
		UAnimMontage* UseAnim = MyPawn->IsFirstPerson() ? Animation.Pawn1P : Animation.Pawn3P;
		if (UseAnim)
		{
			MyPawn->StopAnimMontage(UseAnim);
		}
	}
}

FVector AZSWeapon::GetCameraAim() const
{
	const AZSPlayerController* const PC = GetInstigatorController<AZSPlayerController>();
	FVector FinalAim = FVector::ZeroVector;

	if (PC)
	{
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		FinalAim = GetInstigator()->GetBaseAimRotation().Vector();
	}

	return FinalAim;
}

FVector AZSWeapon::GetAdjustedAim() const
{
	const AZSPlayerController* const PC = GetInstigatorController<AZSPlayerController>();
	FVector FinalAim = FVector::ZeroVector;

	// If we have a player controller use it for the aim
	if (PC)
	{
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}
	else if (GetInstigator())
	{
		/*~
		// Now see if we have an AI controller - we will want to get the aim from there if we do
		AShooterAIController* AIPC = MyPawn ? Cast<AShooterAIController>(MyPawn->Controller) : NULL;
		if (AIPC != NULL)
		{
			FinalAim = AIPC->GetControlRotation().Vector();
		}
		else
		{*/
		FinalAim = GetInstigator()->GetBaseAimRotation().Vector();
		//~}
	}

	return FinalAim;
}

FVector AZSWeapon::GetCameraDamageStartLocation(const FVector& AimDir) const
{
	AZSPlayerController* PC = MyPawn ? Cast<AZSPlayerController>(MyPawn->Controller) : nullptr;
	//~ AShooterAIController* AIPC = MyPawn ? Cast<AShooterAIController>(MyPawn->Controller) : NULL;
	FVector OutStartTrace = FVector::ZeroVector;

	if (PC)
	{
		// use player's camera
		FRotator UnusedRot;
		PC->GetPlayerViewPoint(OutStartTrace, UnusedRot);

		// Adjust trace so there is nothing blocking the ray between the camera and the pawn, and calculate distance from adjusted start
		OutStartTrace = OutStartTrace + AimDir * ((GetInstigator()->GetActorLocation() - OutStartTrace) | AimDir);
	}
	/*~
	else if (AIPC)
	{
		OutStartTrace = GetMuzzleLocation();
	}
	*/

	return OutStartTrace;
}

FVector AZSWeapon::GetMuzzleLocation() const
{
	USkeletalMeshComponent* UseMesh = GetWeaponMesh();
	return UseMesh->GetSocketLocation(MuzzleAttachPoint);
}

FVector AZSWeapon::GetMuzzleDirection() const
{
	USkeletalMeshComponent* UseMesh = GetWeaponMesh();
	return UseMesh->GetSocketRotation(MuzzleAttachPoint).Vector();
}

FHitResult AZSWeapon::WeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const
{

	// Perform trace to retrieve hit info
	const bool bWantComplexTrace = true;
	const AActor* ActorToIgnore = GetInstigator();
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), bWantComplexTrace, ActorToIgnore);
	TraceParams.bReturnPhysicalMaterial = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, EndTrace, COLLISION_WEAPON, TraceParams);

	return Hit;
}

void AZSWeapon::SetOwningPawn(AZSCharacter* NewOwner)
{
	if (MyPawn != NewOwner)
	{
		SetInstigator(NewOwner);
		MyPawn = NewOwner;
		// net owner for RPC calls
		SetOwner(NewOwner);
	}
}


//////////////////////////////////////////////////////////////////////////
// Replication & effects

void AZSWeapon::OnRep_MyPawn()
{
	if (MyPawn)
	{
		OnEnterInventory(MyPawn);
	}
	else
	{
		OnLeaveInventory();
	}
}

void AZSWeapon::OnRep_BurstCounter()
{
	if (BurstCounter > 0)
	{
		SimulateWeaponFire();
	}
	else
	{
		StopSimulatingWeaponFire();
	}
}

void AZSWeapon::OnRep_Reload()
{
	if (bPendingReload)
	{
		StartReload(true);
	}
	else
	{
		StopReload();
	}
}

// Pre-conditions: 
// If playing with a dedicated server net mode, function is being called on a client machine
// Otherwise net mode is either listen server or standalone
void AZSWeapon::SimulateWeaponFire()
{
	// Stop simulation only if authority knows that weapon is not firing
	if (GetLocalRole() == ROLE_Authority && CurrentState != EWeaponState::Firing)
	{
		return;
	}

	if (MuzzleFX)
	{
		USkeletalMeshComponent* UseWeaponMesh = GetWeaponMesh();

		// do not play fx if looping fx already exists
		if (bLoopedMuzzleFX == false || MuzzlePSC == nullptr)
		{
			// Split screen requires we create 2 effects. One that we see and one that the other player sees.
			if (MyPawn && MyPawn->IsLocallyControlled())
			{
				AController* PlayerCon = MyPawn->GetController();
				if (PlayerCon)
				{
					//~ Mesh1P->GetSocketLocation(MuzzleAttachPoint);
					MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, Mesh1P, MuzzleAttachPoint);
					MuzzlePSC->bOwnerNoSee = false;
					MuzzlePSC->bOnlyOwnerSee = true;

					//~ Mesh3P->GetSocketLocation(MuzzleAttachPoint);
					MuzzlePSCSecondary = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, Mesh3P, MuzzleAttachPoint);
					MuzzlePSCSecondary->bOwnerNoSee = true;
					MuzzlePSCSecondary->bOnlyOwnerSee = false;
				}
			}
			else
			{
				MuzzlePSC = UGameplayStatics::SpawnEmitterAttached(MuzzleFX, UseWeaponMesh, MuzzleAttachPoint);
			}
		}
	}

	// do not play fire anim if looping fire anim is already playing
	if (!(bLoopedFireAnim && bPlayingFireAnim)) 
	{
		PlayWeaponAnimation(FireAnim);
		bPlayingFireAnim = true;
	}

	// do not play fire sound if looping fire sound is already playing
	// has separate branches for looping and not looping because one needs to create an audio component
	if (bLoopedFireSound)
	{
		if (FireAC == nullptr)
		{
			FireAC = PlayWeaponSound(FireLoopSound);
		}
	}
	else
	{
		PlayWeaponSound(FireSound);
	}

	// only perform camera shake for human player owned pawns
	AZSPlayerController* PC = MyPawn ? Cast<AZSPlayerController>(MyPawn->Controller) : nullptr;
	if (PC && PC->IsLocalController())
	{
		if (FireCameraShake)
		{
			PC->ClientPlayCameraShake(FireCameraShake, 1);
		}
		if (FireForceFeedback && PC->IsVibrationEnabled())
		{
			FForceFeedbackParameters FFParams;
			FFParams.Tag = TEXT("Weapon");
			PC->ClientPlayForceFeedback(FireForceFeedback, FFParams);
		}
	}
}

void AZSWeapon::StopSimulatingWeaponFire()
{
	if (bLoopedMuzzleFX)
	{
		if (MuzzlePSC)
		{
			MuzzlePSC->DeactivateSystem();
			MuzzlePSC = nullptr;
		}
		if (MuzzlePSCSecondary)
		{
			MuzzlePSCSecondary->DeactivateSystem();
			MuzzlePSCSecondary = nullptr;
		}
	}

	if (bLoopedFireAnim && bPlayingFireAnim)
	{
		StopWeaponAnimation(FireAnim);
		bPlayingFireAnim = false;
	}

	if (FireAC)
	{
		FireAC->FadeOut(0.1f, 0.0f); // takes 0.1 seconds to fade to 0 volume
		FireAC = nullptr;

		PlayWeaponSound(FireFinishSound);
	}
}

void AZSWeapon::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZSWeapon, MyPawn);
	
	DOREPLIFETIME_CONDITION(AZSWeapon, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AZSWeapon, CurrentAmmoInClip, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(AZSWeapon, BurstCounter, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AZSWeapon, bPendingReload, COND_SkipOwner);
}


//////////////////////////////////////////////////////////////////////////
// Reading data

EWeaponState AZSWeapon::GetCurrentState() const
{
	return CurrentState;
}

int32 AZSWeapon::GetCurrentAmmo() const
{
	return CurrentAmmo;
}

int32 AZSWeapon::GetCurrentAmmoInClip() const
{
	return CurrentAmmoInClip;
}

int32 AZSWeapon::GetAmmoPerClip() const
{
	return WeaponConfig.AmmoPerClip;
}

int32 AZSWeapon::GetMaxAmmo() const
{
	return WeaponConfig.MaxAmmo;
}

USkeletalMeshComponent* AZSWeapon::GetWeaponMesh() const
{
	return (MyPawn && MyPawn->IsFirstPerson()) ? Mesh1P : Mesh3P;
}

AZSCharacter* AZSWeapon::GetPawnOwner() const
{
	return MyPawn;
}

bool AZSWeapon::IsEquipped() const
{
	return bIsEquipped;
}

bool AZSWeapon::IsAttachedToPawn() const
{
	return bIsEquipped || bPendingEquip;
}

bool AZSWeapon::HasInfiniteAmmo() const
{
	//~ const AZSPlayerController* MyPC = MyPawn ? Cast<const AZSPlayerController>(MyPawn->Controller) : nullptr;
	return WeaponConfig.bInfiniteAmmo /*~|| (MyPC && MyPC->HasInfiniteAmmo())*/;
}

bool AZSWeapon::HasInfiniteClip() const
{
	//~ const AZSPlayerController* MyPC = MyPawn ? Cast<const AZSPlayerController>(MyPawn->Controller) : nullptr;
	return WeaponConfig.bInfiniteClip /*~|| (MyPC && MyPC->HasInfiniteClip())*/;
}

float AZSWeapon::GetEquipStartedTime() const
{
	return EquipStartedTime;
}

float AZSWeapon::GetEquipDuration() const
{
	return EquipDuration;
}
