///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/GameServiceConfig.h"

#include "WeekendGameService.h"
#include "Engine/World.h"
#include "GameService/GameServiceManager.h"
#include "GameService/WorldGameServiceRunner.h"
#include "UObject/Package.h"

UGameServiceConfig& UGameServiceConfig::CreateForWorld(const UWorld& World, TFunction<void(UGameServiceConfig&)> ConfigExec)
{
	UGameServiceConfig* NewConfig = NewObject<UGameServiceConfig>(World.GetGameInstance());
	if (ConfigExec != nullptr && ConfigExec.IsSet())
	{
		ConfigExec(*NewConfig);
	}
	NewConfig->RegisterWithGameServiceManager(World);
	return *NewConfig;
}

UGameServiceConfig& UGameServiceConfig::CreateForNextWorld(TFunction<void(UGameServiceConfig&)> ConfigExec)
{
	UGameServiceConfig* NewConfig = NewObject<UGameServiceConfig>(GetTransientPackage());
	if (ConfigExec != nullptr && ConfigExec.IsSet())
	{
		ConfigExec(*NewConfig);
	}
	UWorldGameServiceRunner::SetServiceConfigForNextWorld(*NewConfig);
	return *NewConfig;
}

UGameServiceConfig& UGameServiceConfig::CreateForNextWorld(const TSubclassOf<UGameServiceConfig> ParentConfigClass, bool bAutoStartServices, TFunction<void(UGameServiceConfig&)> ConfigExec)
{
	UGameServiceConfig* NewConfig = NewObject<UGameServiceConfig>(GetTransientPackage());
	NewConfig->ParentConfigClass = ParentConfigClass;
	if (ConfigExec != nullptr && ConfigExec.IsSet())
	{
		ConfigExec(*NewConfig);
	}
	UWorldGameServiceRunner::SetServiceConfigForNextWorld(*NewConfig, bAutoStartServices);
	return *NewConfig;
}

void UGameServiceConfig::RegisterWithGameServiceManager(const UWorld& World) const
{
	UE_LOG(LogGameService, Verbose, TEXT("Registering GameServiceConfig (%s) with GameServiceManager"), *GetName());
	UGameServiceManager::SummonInstance(&World).RegisterServices(*this);
}

void UGameServiceConfig::ValidateDependenciesForConfiguredServices() const
{
	for (const TTuple<TSubclassOf<UObject>, TSubclassOf<UGameServiceBase>>& ServiceItr : ConfiguredServices)
	{
		const UGameServiceBase* ServiceInstance = ConfiguredTemplates.Contains(ServiceItr.Key)
			? ConfiguredTemplates[ServiceItr.Key].Get()
			: GetDefault<UGameServiceBase>(ServiceItr.Value);
		CheckServiceDependencies(*ServiceInstance);
	}

	if (ParentConfigClass != nullptr)
	{
		GetDefault<UGameServiceConfig>(ParentConfigClass)->ValidateDependenciesForConfiguredServices();
	}
}

int32 UGameServiceConfig::GetNumConfiguredServices() const
{
	int32 NumConfiguredServices = ConfiguredServices.Num();
	if (ParentConfigClass != nullptr)
	{
		NumConfiguredServices += GetDefault<UGameServiceConfig>(ParentConfigClass)->GetNumConfiguredServices();
	}
	return NumConfiguredServices;
}

TMap<FGameServiceClass, FGameServiceInstanceClass> UGameServiceConfig::GetConfiguredServices() const
{
	TMap<FGameServiceClass, FGameServiceInstanceClass> Result;
	if (ParentConfigClass != nullptr)
	{
		Result = GetDefault<UGameServiceConfig>(ParentConfigClass)->GetConfiguredServices();
	}
	Result.Append(ConfiguredServices);
	return Result;
}

const UGameServiceBase* UGameServiceConfig::GetConfiguredServiceTemplate(const FGameServiceClass& RegisterClass) const
{
	if (ConfiguredTemplates.Contains(RegisterClass))
		return ConfiguredTemplates[RegisterClass].Get();

	return ParentConfigClass != nullptr
		? GetDefault<UGameServiceConfig>(ParentConfigClass)->GetConfiguredServiceTemplate(RegisterClass)
		: nullptr;
}

void UGameServiceConfig::ResetConfiguredServices()
{
	ConfiguredServices.Reset();
	ConfiguredTemplates.Reset();
}

void UGameServiceConfig::CheckServiceDependencies(const UGameServiceBase& ServiceInstance) const
{
	const TMap<FGameServiceClass, FGameServiceInstanceClass> AllConfiguredServices = GetConfiguredServices();
	for (TSubclassOf<UObject> ServiceClassDependency : ServiceInstance.GetServiceClassDependencies())
	{
		checkf(AllConfiguredServices.Contains(ServiceClassDependency), TEXT("%s configured a GameServiceDependency to %s, which was not configured in %s"),
			*ServiceInstance.GetClass()->GetName(), *GetNameSafe(ServiceClassDependency), *GetName());
	}
}
