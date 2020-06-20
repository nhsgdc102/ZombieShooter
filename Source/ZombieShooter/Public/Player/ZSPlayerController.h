// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZSPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class ZOMBIESHOOTER_API AZSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AZSPlayerController();

	/** sets the produce force feedback flag. */
	void SetIsVibrationEnabled(bool bEnable);

	bool IsVibrationEnabled() const;

	/** check if gameplay related actions (movement, weapon usage, etc) are allowed right now */
	bool IsGameInputAllowed() const;

protected:
	/** should produce force feedback? */
	uint8 bIsVibrationEnabled : 1;
};
