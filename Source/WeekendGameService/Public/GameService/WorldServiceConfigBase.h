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
#include "GameService/GameServiceConfig.h"

#include "WorldServiceConfigBase.generated.h"

class UGameServiceWorldBootstrapper;

/**
 * Base class for configurations whose CDO will be automatically registered with the @UGameServiceManager.
 * Derived configurations should define at least one world name pattern.
 * When a world with matching name is entered, the @UWorldGameServiceRunner will automatically register
 * the CDO of the config with the service manager.
 */
UCLASS(Abstract)
class WEEKENDGAMESERVICE_API UWorldServiceConfigBase : public UGameServiceConfig
{
	GENERATED_BODY()

public:
	/** @returns the CDO of the world service config class configured for given world, or nullptr. */
	static const UWorldServiceConfigBase* FindConfigForWorld(const UWorld& World);
	
	/** Configures a game service world bootstrapper which will be run before any service was started. */
	template<class BootstrapperClass>
	void AddBootstrapper()
	{
		static_assert(TIsDerivedFrom<BootstrapperClass, UGameServiceWorldBootstrapper>::Value);
		check(!BootstrapperClass::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
		WorldBootstrappers.Emplace(BootstrapperClass::StaticClass());
	}
	
	/** @returns all the bootstrapper classes configured on this config. */
	FORCEINLINE const TArray<TSubclassOf<UGameServiceWorldBootstrapper>>& GetWorldBootstrappers() const
	{
		return WorldBootstrappers;
	}

protected:
	UPROPERTY(VisibleAnywhere, Category = "GameServices")
	TArray<TSubclassOf<UGameServiceWorldBootstrapper>> WorldBootstrappers;
	
	/** Registers this config class to be used for worlds whose name contains given string (case-insensitive). */
	void RegisterForWorldsWhoseNamesContain(const FString& PartOfWorldName);
};

