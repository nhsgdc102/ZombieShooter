// Fill out your copyright notice in the Description page of Project Settings.


#include "ZSCharacterMovement.h"
#include "ZSCharacter.h"

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
