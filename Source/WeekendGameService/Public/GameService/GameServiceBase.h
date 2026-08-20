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
#include "GameService/GameServiceUser.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "Utils/EnumUtils.h"

#include "GameServiceBase.generated.h"

/** Defines how long a game service will stay alive. */
UENUM()
enum class EGameServiceLifetime : uint8
{
	/** The game service shuts down when the current world tears down. */
	ShutdownWithWorld,

	/** The game service shuts down together with the game instance. */
	ShutdownWithGameInstance
};

DEFINE_ENUM_STRING_CONVERTERS(GameServiceLifetime, EGameServiceLifetime);

/**
 * Base class for all game services.
 * Provides an interface for @UGameServiceManager to start, stop and (potentially) tick the service.
 * Each derived service class also gains access to the inherited GameServiceUser API to configure
 * and access dependencies to other services or subsystems.
 */
UCLASS(Abstract, EditInlineNew, CollapseCategories)
class WEEKENDGAMESERVICE_API UGameServiceBase : public UObject, public FGameServiceUser
{
	GENERATED_BODY()

public:
	// - FGameServiceUser
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const override
	{
		return FGameServiceUserConfig(this);
	}
	// --

	/**
	 * Called when the service is started. All configured service dependencies are promised to already run at this point.
	 * Services can be accessed via @UseService<>() or @FindOptionalService<>(), but subsystem dependencies might not yet
	 * be available. Consider using @WaitForDependencies() in this case.
	 */
	virtual void StartService() {}

	/** @returns whether this service should tick or not. If so, the TickService() method will be called automatically. */
	virtual bool IsTickable() const { return false; }

	/** Called each tick after the service has been started, when IsTickable() returns true. */
	virtual void TickService(float DeltaTime) {}

	/**
	 * Called before the service is destroyed, to provide an opportunity to stop function and clean up.
	 * All configured service dependencies are promised to be still alive and running when this is called.
	 */
	virtual void ShutdownService() {}

	/** @returns optional information about the status of this service, mainly for debugging purposes. */
	virtual TOptional<FString> GetServiceStatusInfo() const { return {}; }

	/** @returns the configured lifetime type, which defines how long the service wants to stay alive. */
	EGameServiceLifetime GetLifetime() const { return Lifetime; }

	/** @returns the configured lifetime type, which defines how long the service wants to stay alive. */
	static EGameServiceLifetime GetLifetimeOf(const TSubclassOf<UGameServiceBase>& ServiceClass)
	{
		return GetDefault<UGameServiceBase>(ServiceClass)->GetLifetime();
	}

protected:
	/** Defines how long a game service will stay alive. */
	EGameServiceLifetime Lifetime = EGameServiceLifetime::ShutdownWithWorld;

	// - FGameServiceUser
	virtual void CheckGameServiceDependencies() const override;
	// --
};

inline void UGameServiceBase::CheckGameServiceDependencies() const
{
	if (Lifetime == EGameServiceLifetime::ShutdownWithWorld)
		return; // World services can have dependencies to anything since they die first.
	for (const TSubclassOf<UObject>& DependencyClass : ConfigureGameServiceUser().ServiceDependencies)
	{
		const UGameServiceBase* ServiceDependency = GetDefault<UGameServiceBase>(DependencyClass.Get());
		if (!ServiceDependency)
			continue;

		checkf(ServiceDependency->Lifetime != EGameServiceLifetime::ShutdownWithWorld,
			TEXT("%s is configured as %s and can thus not depend on world service %s"),
			*GetNameSafe(this), *LexToString(ServiceDependency->Lifetime), *GetNameSafe(DependencyClass.Get()));
	}
}

using FGameServiceClass = TSubclassOf<UObject>;
using FGameServiceInstanceClass = TSubclassOf<UGameServiceBase>;
