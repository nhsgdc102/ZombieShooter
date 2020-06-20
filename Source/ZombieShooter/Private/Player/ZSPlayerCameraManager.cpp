// Fill out your copyright notice in the Description page of Project Settings.


#include "ZombieShooter/Public/Player/ZSPlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "ZombieShooter/Public/Player/ZSCharacter.h"

AZSPlayerCameraManager::AZSPlayerCameraManager()
{
	NormalFOV = 90.0f;
	TargetingFOV = 60.0f;
	ViewPitchMin = -87.0f;
	ViewPitchMax = 87.0f;
	bAlwaysApplyModifiers = true;
}

void AZSPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	AZSCharacter* MyPawn = PCOwner ? Cast<AZSCharacter>(PCOwner->GetPawn()) : nullptr;
	if (MyPawn && MyPawn->IsFirstPerson())
	{
		const float TargetFOV = MyPawn->IsTargeting() ? TargetingFOV : NormalFOV;
		DefaultFOV = FMath::FInterpTo(DefaultFOV, TargetFOV, DeltaTime, 20.0f);
	}

	Super::UpdateCamera(DeltaTime);

	if (MyPawn && MyPawn->IsFirstPerson())
	{
		MyPawn->OnCameraUpdate(GetCameraLocation(), GetCameraRotation());
	}
}
