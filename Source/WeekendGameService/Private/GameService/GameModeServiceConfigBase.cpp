///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/GameModeServiceConfigBase.h"

#include "GameMapsSettings.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameService/Settings/GameServiceFrameworkSettings.h"

namespace
{
	TMap<TSubclassOf<AGameModeBase>, TSubclassOf<UGameModeServiceConfigBase>> GConfigClassesByGameModes = {};
}

void UGameModeServiceConfigBase::RegisterFor(const TSubclassOf<AGameModeBase>& GameModeClass)
{
	ensureMsgf(GameModeClass != AGameModeBase::StaticClass(),
		TEXT("Cannot register %s for AGameModeBase, because it would be configured for ALL worlds, including AutomationTestWorlds. Please configure a subclass of AGameModeBase."), *GetName());

	if (!HasAnyFlags(RF_ClassDefaultObject))
		return;

	TSubclassOf<UGameModeServiceConfigBase>& RegisteredConfigClass = GConfigClassesByGameModes.FindOrAdd(GameModeClass);
	if (RegisteredConfigClass != nullptr && RegisteredConfigClass != GetClass())
	{
		// Only one AutoRegisteredGameServiceConfig per game mode is allowed:
		ensureMsgf(false, TEXT("GameServiceConfig for GameMode %s already has another config class configured!"), *GameModeClass->GetName());
	}

	ConfiguredGameModes.Add(GameModeClass);
	RegisteredConfigClass = GetClass();
}

bool UGameModeServiceConfigBase::ShouldUseWithGameMode(const TSubclassOf<AGameModeBase>& GameModeClass) const
{
	const UGameModeServiceConfigBase* Config = FindConfigForGameModeClass(GameModeClass);
	return IsValid(Config) && this->IsA(Config->GetClass());
}

const UGameModeServiceConfigBase* UGameModeServiceConfigBase::FindConfigForWorld(const UWorld& World)
{
	TSubclassOf<AGameModeBase> CurrentGameModeClass = World.GetWorldSettings()->DefaultGameMode;
	if (!IsValid(CurrentGameModeClass))
	{
		// No override configured in the World Settings. Fallback to default
		const FSoftClassPath& GameMode = UGameMapsSettings::GetGlobalDefaultGameMode();
		CurrentGameModeClass = GameMode.TryLoadClass<AGameModeBase>();
		ensureAlways(IsValid(CurrentGameModeClass));
	}

	return FindConfigForGameModeClass(CurrentGameModeClass);
}

const UGameModeServiceConfigBase* UGameModeServiceConfigBase::FindConfigForGameModeClass(const TSubclassOf<AGameModeBase>& GameModeClass)
{
	for (const auto& Itr : GConfigClassesByGameModes)
	{
		const TSubclassOf<AGameModeBase> ConfiguredGameMode = Itr.Key;
		const TSubclassOf<UGameModeServiceConfigBase> ConfiguredConfig = Itr.Value;

		if (GameModeClass->IsChildOf(ConfiguredGameMode))
			return ConfiguredConfig->GetDefaultObject<UGameModeServiceConfigBase>();
	}

	return nullptr; // No config found for game mode.
}

void UGameModeServiceConfigBase::Reconfigure()
{
	ResetConfiguredServices();

	while (ConfiguredGameModes.Num() > 0)
	{
		if (const TSubclassOf<AGameModeBase> GameModeClass = ConfiguredGameModes.Pop();
			GConfigClassesByGameModes.Contains(GameModeClass) &&
			GConfigClassesByGameModes[GameModeClass] == GetClass())
		{
			GConfigClassesByGameModes.Remove(GameModeClass);
		}
	}

	Configure();
}

void UGameModeServiceConfigBase::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) && !GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		Reconfigure();
		GetMutableDefault<UGameServiceFrameworkSettings>()->RegisterServiceConfig(*this);
	}
}

#if WITH_EDITOR
void UGameModeServiceConfigBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Reconfigure();
}
#endif
