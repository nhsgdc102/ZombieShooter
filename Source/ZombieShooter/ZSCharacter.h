// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZSCharacter.generated.h"

class UCameraComponent;

UCLASS(ABSTRACT)
class ZOMBIESHOOTER_API AZSCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	// Sets default values for this character's properties
	AZSCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	//////////////////////////////////////////////////////////////////////////
	// Input handlers

public:

	/* set up pawn specific input handlers */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/**
	* Move foward/back
	*
	* @param Val Movement input to apply
	*/
	void MoveForward(float Val);

	/**
	* Move right/left
	*
	* @param Val Movement input to apply
	*/
	void MoveRight(float Val);

	/* Frame rate independent turn */
	void TurnAtRate(float Val);

	/* Frame rate independent lookup */
	void LookUpAtRate(float Val);

	/** player pressed start fire action */
	//void OnStartFire();

	/** player released start fire action */
	//void OnStopFire();

	/** player pressed targeting action */
	//void OnStartTargeting();

	/** player released targeting action */
	//void OnStopTargeting();

	/** player pressed reload action */
	//void OnReload();

	/** player pressed jump action */
	void OnStartJump();

	/** player released jump action */
	void OnStopJump();

	/** player pressed run action */
	void OnStartRunning();

	/** player pressed toggled run action */
	void OnStartRunningToggle();

	/** player released run action */
	void OnStopRunning();

protected:

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	float BaseTurnRate;

	/** Base lookup rate, in deg/sec. Other scaling may affect final lookup rate. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	float BaseLookUpRate;

	//////////////////////////////////////////////////////////////////////////
	// Movement

public:

	/** [server + local] change running state */
	void SetRunning(bool bNewRunning, bool bToggle);


	//////////////////////////////////////////////////////////////////////////
	// Reading data

public:

	/** get running state */
	UFUNCTION(BlueprintCallable, Category = Pawn)
	bool IsRunning() const;

	/** get the modifier value for running speed */
	UFUNCTION(BlueprintCallable, Category = Pawn)
	float GetRunningSpeedModifier() const;

protected:

	/** modifier for max movement speed */
	UPROPERTY(EditDefaultsOnly, Category = Pawn)
		float RunningSpeedModifier;

	/** current running state */
	UPROPERTY(Transient, Replicated)
	uint8 bWantsToRun : 1;

	/** from gamepad running is toggled */
	uint8 bWantsToRunToggled : 1;

private:
	/** 1st person camera component */
	UPROPERTY(VisibleDefaultsOnly)
	UCameraComponent* Camera1P;

	/** pawn mesh: 1st person view */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* Mesh1P;

protected:
	/** Returns Mesh1P subobject **/
	FORCEINLINE USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }

	//////////////////////////////////////////////////////////////////////////
	// Server side functions

protected:

	/** update running state */
	UFUNCTION(reliable, server, WithValidation)
		void ServerSetRunning(bool bNewRunning, bool bToggle);
};
