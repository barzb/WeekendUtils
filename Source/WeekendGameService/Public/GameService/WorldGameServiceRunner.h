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
#include "Subsystems/WorldSubsystem.h"

#include "WorldGameServiceRunner.generated.h"

class UGameServiceConfig;

/**
 * The singleton instance of this subsystem for each world will take care of maintaining the
 * game service world in cooperation with the persistent @UGameServiceManager.
 *
 * The runner will do the following things:
 * - Register @UGameModeServiceConfigBase objects that apply to the current world
 * - Make sure all @UWorldSubsystem dependencies of configured game services are available
 * - Start all configured game service world bootstrapper
 * - Start all configured game services for the current world in the correct order
 * - Tick all running game services (that want to be ticked)
 * - Shutdown relevant running services when the world tears down
 * - Clears relevant registered service configs when the world tears down
 */
UCLASS(MinimalAPI)
class UWorldGameServiceRunner final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// - UTickableWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
	// --


	/** Sets a specific @UGameServiceConfig instance to be used when the next world is starting. */
	static void SetServiceConfigForNextWorld(UGameServiceConfig& ServiceConfig);
	static void SetServiceConfigForNextWorld(UGameServiceConfig& ServiceConfig, bool bAutoStartServices);

private:
	void RegisterAutoServiceConfigs();
	void RunWorldBootstrappers();
	void StartRegisteredServices();
	void TickRunningServices(float DeltaTime);
	TArray<TSubclassOf<UWorldSubsystem>> GatherWorldSubsystemDependencies() const;

	bool bAutoStartServicesForNextWorld = true; 
};
