///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGameUtils.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameService/GameServiceLocator.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "SaveGame/SaveGamePreset.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"
#include "SaveGame/SaveGameService.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ISettingsModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSaveGameUtils, Log, All);

namespace
{
	using FSlotName = FString;

#if WITH_EDITOR
	const FString PIE_SETTINGS_INI_FILE = GGameUserSettingsIni;
	const TCHAR*  PIE_SETTINGS_INI_SECTION = TEXT("WeekendUtils.SaveGameUtils");
	const TCHAR*  PIE_SETTING_OVERRIDE_SLOT_NAME = TEXT("OverridePlayInEditorSaveGameSlotName");
	const TCHAR*  PIE_SETTING_SHOULD_OVERRIDE_SLOT = TEXT("ShouldOverridePlayInEditorSaveGameSlot");
	const TCHAR*  PIE_SETTING_OVERRIDE_SAVELOADBEHAVIOR = TEXT("OverrideSaveLoadBehavior");
	const TCHAR*  PIE_SETTING_SHOULD_OVERRIDE_SAVELOADBEHAVIOR = TEXT("ShouldOverrideSaveLoadBehavior");
#endif

	FSoftObjectPath GetLevel(UWorld* World)
	{
#if WITH_EDITOR
		World = (IsValid(World) ? World : GEditor->PlayWorld.Get());
#endif
		return FSoftObjectPath(GetPathNameSafe(World));
	}

	TSubclassOf<AGameModeBase> GetGameModeClass(UWorld* World)
	{
#if WITH_EDITOR
		World = (IsValid(World) ? World : GEditor->PlayWorld.Get());
#endif
		return (IsValid(World) ? World->GetWorldSettings()->DefaultGameMode : nullptr);
	}
}

void USaveGameUtils::OpenSaveGameProjectSettings()
{
#if WITH_EDITOR
	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(
		Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
#else
	unimplemented();
#endif
}

void USaveGameUtils::GetOverridePlayInEditorSaveGameSlot(bool& bOutIsOverridden, FString& OutSlotName)
{
#if WITH_EDITOR
	const FString SectionName = StaticClass()->GetName();
	bool bSuccess = true;
	bSuccess &= GConfig->GetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SLOT_NAME, OUT OutSlotName, PIE_SETTINGS_INI_FILE);
	bSuccess &= GConfig->GetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SLOT, OUT bOutIsOverridden, PIE_SETTINGS_INI_FILE);
	bOutIsOverridden &= bSuccess;
#else
	unimplemented();
#endif
}

void USaveGameUtils::SetOverridePlayInEditorSaveGameSlot(bool bOverride, FString SlotName)
{
#if WITH_EDITOR
	GConfig->SetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SLOT, bOverride, PIE_SETTINGS_INI_FILE);
	GConfig->SetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SLOT_NAME, *SlotName, PIE_SETTINGS_INI_FILE);
	GConfig->Flush(false, PIE_SETTINGS_INI_FILE);
#else
	unimplemented();
#endif
}

void USaveGameUtils::GetOverrideSaveLoadBehavior(bool& bOutIsOverridden, FSoftClassPath& OutBehaviorClass)
{
#if WITH_EDITOR
	const FString SectionName = StaticClass()->GetName();
	bool bSuccess = true;
	FString BehaviorClassString;
	bSuccess &= GConfig->GetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SAVELOADBEHAVIOR, OUT bOutIsOverridden, PIE_SETTINGS_INI_FILE);
	bSuccess &= GConfig->GetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SAVELOADBEHAVIOR, OUT BehaviorClassString, PIE_SETTINGS_INI_FILE);
	OutBehaviorClass = FSoftClassPath(BehaviorClassString);
	bOutIsOverridden &= bSuccess && !OutBehaviorClass.IsNull();
#else
	unimplemented();
#endif
}

void USaveGameUtils::SetOverrideSaveLoadBehavior(bool bOverride, FSoftClassPath SetOverrideSaveLoadBehavior)
{
#if WITH_EDITOR
	GConfig->SetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SAVELOADBEHAVIOR, bOverride, PIE_SETTINGS_INI_FILE);
	GConfig->SetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SAVELOADBEHAVIOR, *SetOverrideSaveLoadBehavior.ToString(), PIE_SETTINGS_INI_FILE);
	GConfig->Flush(false, PIE_SETTINGS_INI_FILE);
#else
	unimplemented();
#endif
}

TArray<FString> USaveGameUtils::FindAllSaveGamePresetNames()
{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	unimplemented();
	return {};
#else
	return USaveGamePreset::CollectSaveGamePresetNames().Array();
#endif
}

TArray<FString> USaveGameUtils::FindAllLocalSaveGameSlotNames()
{
	TArray<FSlotName> Result = {};
	{
		TArray<FString> FoundFiles;
		IFileManager::Get().FindFiles(OUT FoundFiles, *FString(FPaths::ProjectSavedDir() / "SaveGames"), TEXT(".sav"));
		Result.Reserve(FoundFiles.Num());
		for (const FString& Filename : FoundFiles)
		{
			Result.Add(FPaths::GetBaseFilename(Filename));
		}
	}
	return Result;
}

bool USaveGameUtils::HasLoadedSaveGame()
{
	const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>();
	if (!SaveGameService)
		return false;
	
	return SaveGameService->GetCurrentSaveGame().WasEverLoaded();
}

ESaveSlotState USaveGameUtils::GetLocalSaveGameSlotState(const FString& SlotName, int32 UserIndex)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
		return ESaveSlotState::Empty;

	const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>();
	if (!SaveGameService)
		return ESaveSlotState::Present;
	
	const TOptional<FSlotName> SlotLastRestoredFrom = SaveGameService->GetCurrentSaveGame().GetSlotLastRestoredFrom();
	const TOptional<FSlotName> SlotLastSavedTo = SaveGameService->GetCurrentSaveGame().GetSlotLastSavedTo();
	if ((!SlotLastRestoredFrom.IsSet() || SlotLastRestoredFrom.GetValue() != SlotName)
		&& (!SlotLastSavedTo.IsSet() || SlotLastSavedTo.GetValue() != SlotName))
		return ESaveSlotState::Present;
	
	return ESaveSlotState::Loaded;
}

void USaveGameUtils::DeleteLocalSaveGame(const FString& SlotName, int32 UserIndex)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
		return;

	UE_LOG(LogSaveGameUtils, Log, TEXT("DeleteLocalSaveGame: Deleting \"%s\" (user %d)"), *SlotName, UserIndex);
	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}

void USaveGameUtils::DeleteAllLocalSaveGames(int32 UserIndex)
{
	for (const FSlotName& SlotName : FindAllLocalSaveGameSlotNames())
	{
		if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
			continue;

		UE_LOG(LogSaveGameUtils, Log, TEXT("DeleteAllLocalSaveGames: Deleting \"%s\" (user %d)"), *SlotName, UserIndex);
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
	}
}

bool USaveGameUtils::IsSavingAllowedForWorld(UWorld* World)
{
	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
	if (Settings->bAlwaysAllowSaving)
		return true;

	const FSoftObjectPath Level = GetLevel(World);

#if WITH_EDITOR
	// Since PIE adds some prefix to the actual path, only compare the asset name in editor.
	if (Algo::FindBy(Settings->MapsWhereSavingIsAllowed, Level.GetAssetName(), &FSoftObjectPath::GetAssetName))
		return true;
#else
	if (Settings->MapsWhereSavingIsAllowed.Contains(Level))
		return true;
#endif

	for (const UClass* GameModeClass = GetGameModeClass(World); GameModeClass != nullptr; GameModeClass = GameModeClass->GetSuperClass())
	{
		const FSoftClassPath GameMode{GetPathNameSafe(GameModeClass)};
		if (Settings->GameModesWhereSavingIsAllowed.Contains(GameMode))
			return true;

		if (GameModeClass == AGameModeBase::StaticClass())
			break;
	}

	return false;
}

TArray<FSoftClassPath> USaveGameUtils::GetAllAvailableSaveLoadBehaviorClasses()
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(USaveLoadBehavior::StaticClass(), OUT ChildClasses);
	return TArray<FSoftClassPath>{ChildClasses};
}
