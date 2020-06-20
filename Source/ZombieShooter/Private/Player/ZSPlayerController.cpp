// Fill out your copyright notice in the Description page of Project Settings.


#include "ZombieShooter/Public/Player/ZSPlayerController.h"
#include "ZombieShooter/Public/Player/ZSPlayerCameraManager.h"

AZSPlayerController::AZSPlayerController()
{
	PlayerCameraManagerClass = AZSPlayerCameraManager::StaticClass();
}

void AZSPlayerController::SetIsVibrationEnabled(bool bEnable)
{
	bIsVibrationEnabled = bEnable;
}

bool AZSPlayerController::IsVibrationEnabled() const
{
	return bIsVibrationEnabled;
}

bool AZSPlayerController::IsGameInputAllowed() const
{
	//~ bAllowGameActions && !bCinematicMode
	return true;
}