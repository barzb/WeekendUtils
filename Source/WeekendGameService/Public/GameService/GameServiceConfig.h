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
#include "GameService/GameServiceBase.h"
#include "GameService/GameServiceUtils.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "GameServiceConfig.generated.h"

/**
 * Configuration container for the @UGameServiceManager.
 */
UCLASS(NotBlueprintable)
class WEEKENDGAMESERVICE_API UGameServiceConfig : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Creates a UGameServiceConfig instance for given world, automatically registering it with the @UGameServiceManager.
	 * @note Be aware that some service users (like WorldSubsystems) could already have started services.
	 * @param World The world used as outer for the new config instance.
	 * @param ConfigExec External function to be called to configure the config instance before it is registered.
	 * @returns the created and already registered service config.
	 *
	 * @example:
	 * UGameServiceConfig::CreateForWorld(World, [](UGameServiceConfig& Config)
	 * {
	 *    Config.SetPriority(7);
	 *    Config.AddService<USomeService>();
	 *    Config.AddService<IAnotherServiceInterface, UAnotherServiceImpl>();
	 * });
	 */
	static UGameServiceConfig& CreateForWorld(const UWorld& World, TFunction<void(UGameServiceConfig&)> ConfigExec);

	/**
	 * Creates a UGameServiceConfig instance for the next world that will start, automatically registering it with the @UGameServiceManager.
	 * @note This is mainly intended to be used in automation tests, before a test world is created.
	 * @param ConfigExec External function to be called to configure the config instance before it is registered.
	 * @returns the created and already registered service config.
	 *
	 * @example:
	 * UGameServiceConfig::CreateForNextWorld([](UGameServiceConfig& Config)
	 * {
	 *    Config.SetPriority(7);
	 *    Config.AddService<USomeService>();
	 *    Config.AddService<IAnotherServiceInterface, UAnotherServiceImpl>();
	 * });
	 */
	static UGameServiceConfig& CreateForNextWorld(TFunction<void(UGameServiceConfig&)> ConfigExec);

	/**
	 * Creates a UGameServiceConfig instance for the next world that will start, automatically registering it with the @UGameServiceManager.
	 * @note This is mainly intended to be used in automation tests, before a test world is created.
	 * @param ParentConfigClass The class of the config that should be used as parent configuration.
	 * @param bAutoStartServices Whether the services should be automatically started when the service manager starts.
	 * @param ConfigExec (Optional) External function to be called to configure the config instance before it is registered.
	 * @returns the created and already registered service config.
	 *
	 * @example:
	 * UGameServiceConfig::CreateForNextWorld<UMyGameServiceConfig>([](UGameServiceConfig& Config)
	 * {
	 *    Config.SetPriority(7);
	 *    Config.AddService<USomeService>();
	 *    Config.AddService<IAnotherServiceInterface, UAnotherServiceImpl>();
	 * });
	 */
	static UGameServiceConfig& CreateForNextWorld(const TSubclassOf<UGameServiceConfig> ParentConfigClass, bool bAutoStartServices, TFunction<void(UGameServiceConfig&)> ConfigExec = nullptr);
	template<class ConfigClass> static UGameServiceConfig& CreateForNextWorld(bool bAutoStartServices, TFunction<void(UGameServiceConfig&)> ConfigExec = nullptr)
	{
		return CreateForNextWorld(ConfigClass::StaticClass(), bAutoStartServices, ConfigExec);
	}

	/** Automatically registers the config instance with the @UGameServiceManager. Already called when using @CreateForWorld(). */
	void RegisterWithGameServiceManager(const UWorld& World) const;

	/** Checks the configured dependencies of each configured service, and asserts for each service that is configured as dependency, but is missing from this config. */
	void ValidateDependenciesForConfiguredServices() const;

	/**
	 * Configures a game service class to be registered.
	 * @note Services are only instanced once (=> singleton) for the register-type.
	 * @note Services that are registered with the same InstanceClass will share the same instance.
	 */
	template<class ServiceClass, class InstanceClass = ServiceClass>
	void AddService()
	{
		static_assert(!TIsAbstract<InstanceClass>::Value);
		static_assert(TIsDerivedFrom<InstanceClass, ServiceClass>::Value);
		static_assert(TIsDerivedFrom<InstanceClass, UGameServiceBase>::Value);
		ConfiguredServices.Add(GameService::GetServiceUClass<ServiceClass>(), InstanceClass::StaticClass());
	}

	/**
	 * Configures a game service class to be registered.
	 * @note Services are only instanced once (=> singleton) for the register-type.
	 * @note Services that are registered with the same InstanceClass will share the same instance.
	 */
	template<class ServiceClass>
	void AddService(const FGameServiceInstanceClass& InstanceClass)
	{
		check(InstanceClass != nullptr);
		check(InstanceClass->IsChildOf<ServiceClass>());
		check(!InstanceClass->HasAnyClassFlags(CLASS_Abstract));
		ConfiguredServices.Add(GameService::GetServiceUClass<ServiceClass>(), InstanceClass);
	}

	/**
	 * Configures a game service class to be registered.
	 * The created service instance will be based on provided template object (can be CDO). 
	 * @note Services are only instanced once (=> singleton) for the register-type.
	 * @note Services that are registered with the same InstanceClass will share the same instance.
	 */
	template<class ServiceClass>
	void AddService(const UGameServiceBase& TemplateInstance)
	{
		check(!TemplateInstance.GetClass()->HasAnyClassFlags(CLASS_Abstract));
		const TSubclassOf<UObject> RegisterClass = GameService::GetServiceUClass<ServiceClass>();
		ConfiguredServices.Add(RegisterClass, TemplateInstance.GetClass());
		ConfiguredTemplates.Add(RegisterClass, &TemplateInstance);
	}

	int32 GetNumConfiguredServices() const;
	TMap<FGameServiceClass, FGameServiceInstanceClass> GetConfiguredServices() const;
	const UGameServiceBase* GetConfiguredServiceTemplate(const FGameServiceClass& RegisterClass) const;

	/** Configs with a higher priority will overwrite service registrations from configs with lower priority. */
	FORCEINLINE void SetPriority(uint32 NewPriority) { ConfiguredPriority = NewPriority; }
	FORCEINLINE uint32 GetPriority() const { return ConfiguredPriority; }

protected:
	/** When set, inherits any service configurations from this parent config. */
	UPROPERTY(EditAnywhere, Category = "GameServices")
	TSubclassOf<UGameServiceConfig> ParentConfigClass = nullptr;

	/** Key: FGameServiceClass | Value: FGameServiceInstanceClass */
	UPROPERTY(VisibleAnywhere, Category = "GameServices")
	TMap<TSubclassOf<UObject>, TSubclassOf<UGameServiceBase>> ConfiguredServices;

	/** Key: FGameServiceClass | Value: UGameService Instance */
	UPROPERTY(VisibleAnywhere, Category = "GameServices")
	TMap<TSubclassOf<UObject>, TObjectPtr<const UGameServiceBase>> ConfiguredTemplates;

	UPROPERTY(VisibleAnywhere, Category = "GameServices")
	uint32 ConfiguredPriority = 0;

	void ResetConfiguredServices();

	void CheckServiceDependencies(const UGameServiceBase& ServiceInstance) const;
};
