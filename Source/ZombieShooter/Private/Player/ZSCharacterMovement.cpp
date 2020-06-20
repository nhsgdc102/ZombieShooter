// Fill out your copyright notice in the Description page of Project Settings.


#include "ZombieShooter/Public/Player/ZSCharacterMovement.h"
#include "ZombieShooter/Public/Player/ZSCharacter.h"

float UZSCharacterMovement::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();

	const AZSCharacter* ZSCharacterOwner = Cast<AZSCharacter>(PawnOwner);
	if (ZSCharacterOwner)
	{
		//if (ShooterCharacterOwner->IsTargeting())
		//{
			//MaxSpeed *= ShooterCharacterOwner->GetTargetingSpeedModifier();
		//}
		if (ZSCharacterOwner->IsRunning())
		{
			MaxSpeed *= ZSCharacterOwner->GetRunningSpeedModifier();
		}
	}

	return MaxSpeed;
}
