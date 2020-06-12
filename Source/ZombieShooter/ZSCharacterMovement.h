// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZSCharacterMovement.generated.h"

/**
 *  For use with zombie shooting characters
 */
UCLASS()
class ZOMBIESHOOTER_API UZSCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:

	virtual float GetMaxSpeed() const override;
};
