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
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGamePreset.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ISettingsModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSaveGameUtils, Log, All);

namespace
{
	using FSlotName = FString;

#if WITH_EDITOR
	const FString& GetPieSettingsIniFile() { return GGameUserSettingsIni; }

	const TCHAR*  PIE_SETTINGS_INI_SECTION = TEXT("WeekendUtils.SaveGameUtils");
	const TCHAR*  PIE_SETTING_OVERRIDE_SLOT_NAME = TEXT("OverridePlayInEditorSaveGameSlotName");
	const TCHAR*  PIE_SETTING_SHOULD_OVERRIDE_SLOT = TEXT("ShouldOverridePlayInEditorSaveGameSlot");
	const TCHAR*  PIE_SETTING_OVERRIDE_SAVELOADBEHAVIOR = TEXT("OverrideSaveLoadBehavior");
	const TCHAR*  PIE_SETTING_SHOULD_OVERRIDE_SAVELOADBEHAVIOR = TEXT("ShouldOverrideSaveLoadBehavior");
#endif

	FSoftObjectPath GetLevel(UWorld* World)
	{
#if WITH_EDITOR
		// (i) GEditor is null in editor-less commandlets, which still compile WITH_EDITOR:
		World = (IsValid(World) ? World : (GEditor ? GEditor->PlayWorld.Get() : nullptr));
#endif
		return FSoftObjectPath(GetPathNameSafe(World));
	}

	TSubclassOf<AGameModeBase> GetGameModeClass(UWorld* World)
	{
#if WITH_EDITOR
		World = (IsValid(World) ? World : (GEditor ? GEditor->PlayWorld.Get() : nullptr));
#endif
		if (!IsValid(World))
			return nullptr;

		const AWorldSettings* WorldSettings = World->GetWorldSettings();
		return (WorldSettings ? WorldSettings->DefaultGameMode : nullptr);
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
	bSuccess &= GConfig->GetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SLOT_NAME, OUT OutSlotName, GetPieSettingsIniFile());
	bSuccess &= GConfig->GetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SLOT, OUT bOutIsOverridden, GetPieSettingsIniFile());
	bOutIsOverridden &= bSuccess;
#else
	unimplemented();
#endif
}

void USaveGameUtils::SetOverridePlayInEditorSaveGameSlot(bool bOverride, FString SlotName)
{
#if WITH_EDITOR
	GConfig->SetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SLOT, bOverride, GetPieSettingsIniFile());
	GConfig->SetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SLOT_NAME, *SlotName, GetPieSettingsIniFile());
	GConfig->Flush(false, GetPieSettingsIniFile());
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
	bSuccess &= GConfig->GetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SAVELOADBEHAVIOR, OUT bOutIsOverridden, GetPieSettingsIniFile());
	bSuccess &= GConfig->GetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SAVELOADBEHAVIOR, OUT BehaviorClassString, GetPieSettingsIniFile());
	OutBehaviorClass = FSoftClassPath(BehaviorClassString);
	bOutIsOverridden &= bSuccess && !OutBehaviorClass.IsNull();
#else
	unimplemented();
#endif
}

void USaveGameUtils::SetOverrideSaveLoadBehavior(bool bOverride, FSoftClassPath SetOverrideSaveLoadBehavior)
{
#if WITH_EDITOR
	GConfig->SetBool(PIE_SETTINGS_INI_SECTION, PIE_SETTING_SHOULD_OVERRIDE_SAVELOADBEHAVIOR, bOverride, GetPieSettingsIniFile());
	GConfig->SetString(PIE_SETTINGS_INI_SECTION, PIE_SETTING_OVERRIDE_SAVELOADBEHAVIOR, *SetOverrideSaveLoadBehavior.ToString(), GetPieSettingsIniFile());
	GConfig->Flush(false, GetPieSettingsIniFile());
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

int64 USaveGameUtils::CalculateObjectSizeForSaveGame(const UObject& Object, EUnit DesiredUnit)
{
	static const TArray AllowedUnits{EUnit::Bytes, EUnit::Kilobytes, EUnit::Megabytes, EUnit::Gigabytes, EUnit::Terabytes};
	if (!ensureAlways(AllowedUnits.Contains(DesiredUnit)))
		return 0.0;

	// Serialize() is non-const but since we're just serializing TO an archive,
	// it doesn't change the object's state so it's acceptable to const_cast:
	UObject* MutableObject = const_cast<UObject*>(&Object);

	TArray<uint8> SerializedData;
	FMemoryWriter MemoryWriter(OUT SerializedData, true);
	if (UModularSaveGame* SaveGameObject = Cast<UModularSaveGame>(MutableObject); IsValid(SaveGameObject))
	{
		// This will also count the modules into the total size of a ModularSaveGame object:
		FWeekendUtilsSubobjectProxyArchive Ar(MemoryWriter, *SaveGameObject);
		SaveGameObject->Serialize(Ar);
	}
	else
	{
		FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, true);
		MutableObject->Serialize(Ar);
	}

	const int64 SizeInBytes = SerializedData.Num();
	return FUnitConversion::Convert(SizeInBytes, EUnit::Bytes, DesiredUnit);
}
