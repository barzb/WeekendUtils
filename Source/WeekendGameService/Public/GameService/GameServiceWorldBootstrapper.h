///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "GameServiceUser.h"
#include "UObject/Object.h"
#include "GameServiceWorldBootstrapper.generated.h"

/**
 * Base class for bootstrapper classes that need to run code before any world- or game mode-bound 
 * game service was started when loading into a world.
 */
UCLASS(Abstract)
class WEEKENDGAMESERVICE_API UGameServiceWorldBootstrapper : public UObject,
															 public FGameServiceUser
{
	GENERATED_BODY()
	
public:
	// - FGameServiceUser
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const override { return FGameServiceUserConfig(this); }
	// --
	
	/** Extension point for running custom bootstrapping logic. */
	virtual void BootstrapOuterWorld(UWorld& OuterWorld) PURE_VIRTUAL(BootstrapOuterWorld, {});
};
