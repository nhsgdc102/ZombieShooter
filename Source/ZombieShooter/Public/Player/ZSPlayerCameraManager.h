// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ZSPlayerCameraManager.generated.h"

/**
 * 
 */
UCLASS()
class ZOMBIESHOOTER_API AZSPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	/** set defaults */
	AZSPlayerCameraManager();

	/** normal FOV */
	float NormalFOV;

	/** targeting FOV */
	float TargetingFOV;

	/** After updating camera, inform pawn to update 1p mesh to match camera's location&rotation */
	virtual void UpdateCamera(float DeltaTime) override;
};
