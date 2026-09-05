///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveLoadBehavior.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGamePreset.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/SaveGameUtils.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"
#include "SaveGame/Modules/SaveGameModule_SaveLoadDebugHistory.h"

DEFINE_LOG_CATEGORY(LogSaveLoadBehavior);

///////////////////////////////////////////////////////////////////////////////////////
/// UTILS

namespace
{
	FSimpleSaveGameHeaderData* TryGetHeaderData(const FCurrentSaveGame& SaveGame)
	{
		const UModularSaveGame* ModularSaveGame = SaveGame.GetPtr<UModularSaveGame>();
		if (!ModularSaveGame)
			return nullptr;

		return ModularSaveGame->GetMutableHeaderDataPtr<FSimpleSaveGameHeaderData>();
	}

	FSimpleSaveGameHeaderData* TryGetHeaderData(const USaveGame& SaveGame)
	{
		const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(&SaveGame);
		if (!ModularSaveGame)
			return nullptr;

		return ModularSaveGame->GetMutableHeaderDataPtr<FSimpleSaveGameHeaderData>();
	}
}

///////////////////////////////////////////////////////////////////////////////////////
/// @USaveLoadBehavior

void USaveLoadBehavior::HandleBeforeSaveGameSaved(const FCurrentSaveGame& SaveGame, USaveGameService* SaveGameService)
{
	if (FSimpleSaveGameHeaderData* HeaderData = TryGetHeaderData(SaveGame))
	{
		HeaderData->SaveCounter++;
		HeaderData->UtcTimeOfLastSave = FDateTime::UtcNow();
		HeaderData->LoadedLevel = FSoftObjectPath(GetWorld()->GetPathName());
	}
}

void USaveLoadBehavior::HandleAfterSaveGameRestored(const FCurrentSaveGame& SaveGame, USaveGameService* SaveGameService)
{
	if (FSimpleSaveGameHeaderData* HeaderData = TryGetHeaderData(SaveGame))
	{
		HeaderData->RestoreCounter++;
		HeaderData->UtcTimeOfLastRestore = FDateTime::UtcNow();
	}
}

bool USaveLoadBehavior::CanContinueCurrentSaveGame(const FCurrentSaveGame& CurrentSaveGame) const
{
	return (CurrentSaveGame.IsValid() && !CurrentSaveGame.IsNewGame());
}

USaveGame& USaveLoadBehavior::CreateNewSavegameObject(USaveGameService& SaveGameService) const
{
	UModularSaveGame* ModularSaveGame = NewObject<UModularSaveGame>(&SaveGameService);
	auto& Module = ModularSaveGame->FindOrAddModule<USaveGameModule_SaveLoadDebugHistory>();
	Module.SaveGameService = &SaveGameService;
	return *ModularSaveGame;
}

USaveGame& USaveLoadBehavior::DuplicateSaveGameObject(USaveGameService& SaveGameService, const USaveGame& SaveGameToCopy) const
{
	const UModularSaveGame* ModularSaveToDuplicate = CastChecked<UModularSaveGame>(&SaveGameToCopy);
	UModularSaveGame* Duplicate = DuplicateObject<UModularSaveGame>(ModularSaveToDuplicate, &SaveGameService);
	Duplicate->ClearFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_DefaultSubObject); // Clear CDO flags from duplication.
	Duplicate->SetInstancedHeaderData(FInstancedStruct(*ModularSaveToDuplicate->GetInstancedHeaderData()));

	return *Duplicate;
}

TSubclassOf<USaveGameSerializer> USaveLoadBehavior::GetSaveGameSerializerClass() const
{
	return UModularSaveGameSerializer::StaticClass();
}

TOptional<FDateTime> USaveLoadBehavior::FindTimeOfLastSaveFromSaveGame(const USaveGame& SaveGame) const
{
	if (const FSimpleSaveGameHeaderData* HeaderData = TryGetHeaderData(SaveGame); (HeaderData && HeaderData->WasEverSaved()))
	{
		return HeaderData->UtcTimeOfLastSave;
	}

	return {};
}

bool USaveLoadBehavior::TryTravelToSavedLevel(const FCurrentSaveGame& SaveGame)
{
	const FSimpleSaveGameHeaderData* HeaderData = TryGetHeaderData(SaveGame);
	if (!HeaderData)
	{
		UE_LOG(LogSaveLoadBehavior, Error, TEXT("Cannot travel to saved level (%s): Unexpected SaveGame structure"), *GetNameSafe(SaveGame.GetPtr()));
		return false;
	}

	if (!HeaderData->LoadedLevel.IsValid())
	{
		UE_LOG(LogSaveLoadBehavior, Error, TEXT("Cannot travel to saved level (%s): LoadedLevel is invalid"), *GetNameSafe(SaveGame.GetPtr()));
		return false;
	}

	const FName LevelToLoad = HeaderData->LoadedLevel.GetLongPackageFName();
	const FString Options = MakeTravelOptions(SaveGame);
	UGameplayStatics::OpenLevel(this, LevelToLoad, true, Options);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////
/// @UDefaultSaveLoadBehavior

UDefaultSaveLoadBehavior::UDefaultSaveLoadBehavior()
{
	SaveSlotNames =
	{
		"GameSlot1", "GameSlot2", "GameSlot3", "GameSlot4",
		"GameSlot5", "GameSlot6", "GameSlot7", "GameSlot8",
	};
}

void UDefaultSaveLoadBehavior::HandleGameStart(USaveGameService& SaveGameService)
{
	SaveGameService.PreloadSaveGamesAsync(GetSaveSlotNamesAllowedForLoading(SaveGameService.GetCurrentSaveGame()), USaveGameService::FOnPreloadCompleted::CreateWeakLambda(this,
		[this, SaveGameService = MakeWeakObjectPtr(&SaveGameService)](const TArray<USaveGame*>& PreloadedSaveGames, const TArray<FSlotName>& PreloadedSlotNames)
		{
			if (!SaveGameService.IsValid())
				return;
			HandlePreloadCompleted(*SaveGameService, PreloadedSaveGames, PreloadedSlotNames);
		}));
}

TSet<USaveLoadBehavior::FSlotName> UDefaultSaveLoadBehavior::GetSaveSlotNamesAllowedForSaving(const FCurrentSaveGame& CurrentSaveGame) const
{
	return SaveSlotNames;
}

TSet<USaveLoadBehavior::FSlotName> UDefaultSaveLoadBehavior::GetSaveSlotNamesAllowedForLoading(const FCurrentSaveGame& CurrentSaveGame) const
{
	TSet<FSlotName> Result = SaveSlotNames;
	if (const FSlotName AutosaveSlotName = GetAutosaveSlotName(CurrentSaveGame); !AutosaveSlotName.IsEmpty())
	{
		Result.Add(AutosaveSlotName);
	}
	return Result;
}

void UDefaultSaveLoadBehavior::HandlePreloadCompleted(USaveGameService& SaveGameService, TArray<USaveGame*> PreloadedSaveGames, TArray<FSlotName> PreloadedSlotNames)
{
	// Attempt to find and restore the most "recent" save game:

	TOptional<FDateTime> MostRecentSaveTime = {};
	TOptional<TPair<FSlotName, USaveGame*>> MostRecentSaveGame = {};
	for (int32 i = 0; i < PreloadedSaveGames.Num(); ++i)
	{
		if (!IsValid(PreloadedSaveGames[i]))
			continue;

		TOptional<FDateTime> TimeOfLastSave = FindTimeOfLastSaveFromSaveGame(*PreloadedSaveGames[i]);
		if (!TimeOfLastSave.IsSet())
			continue;

		if (!MostRecentSaveTime.IsSet() || ((*TimeOfLastSave) > (*MostRecentSaveTime)))
		{
			MostRecentSaveTime = TimeOfLastSave;
			MostRecentSaveGame = { PreloadedSlotNames[i], PreloadedSaveGames[i] };
		}
	}

	if (MostRecentSaveGame.IsSet())
	{
		USaveGame* SaveGameToMakeCurrent = MostRecentSaveGame->Value;
		UE_LOG(LogSaveLoadBehavior, Log, TEXT("Preload completed. Loading most recent save game (%s), saved on (UTC) %s"),
			*GetNameSafe(SaveGameToMakeCurrent), *MostRecentSaveTime->ToString());
		SaveGameService.RestoreAsCurrentSaveGame(*SaveGameToMakeCurrent, MostRecentSaveGame->Key);
	}
}

///////////////////////////////////////////////////////////////////////////////////////
/// @UDefaultPlayInEditorSaveLoadBehavior

void UDefaultPlayInEditorSaveLoadBehavior::HandleGameStart(USaveGameService& SaveGameService)
{
#if WITH_EDITOR
	const FSlotName SlotName = GetPlayInEditorSlotName();
	if (SlotName.IsEmpty())
		return;

	if (SaveGameService.DoesSaveFileExist(SlotName))
	{
		SaveGameService.TryLoadCurrentSaveGameFromSlotSynchronous(SlotName);
	}
	else if (const USaveGamePreset* Preset = USaveGamePreset::FindSaveGamePreset(SlotName))
	{
		Preset->RestoreAsCurrentSaveGame(SaveGameService);
	}
#else
	unimplemented();
#endif
}

TSet<USaveLoadBehavior::FSlotName> UDefaultPlayInEditorSaveLoadBehavior::GetSaveSlotNamesAllowedForSaving(const FCurrentSaveGame&) const
{
	return { GetPlayInEditorSlotName() };
}

TSet<USaveLoadBehavior::FSlotName> UDefaultPlayInEditorSaveLoadBehavior::GetSaveSlotNamesAllowedForLoading(const FCurrentSaveGame&) const
{
	return { GetPlayInEditorSlotName() };
}

USaveLoadBehavior::FSlotName UDefaultPlayInEditorSaveLoadBehavior::GetAutosaveSlotName(const FCurrentSaveGame&) const
{
	return GetPlayInEditorSlotName();
}

USaveLoadBehavior::FSlotName UDefaultPlayInEditorSaveLoadBehavior::GetPlayInEditorSlotName() const
{
	bool bUseOverrideSlot = false;
	FSlotName OverrideSlotName = "";
	USaveGameUtils::GetOverridePlayInEditorSaveGameSlot(OUT bUseOverrideSlot, OUT OverrideSlotName);
	return (bUseOverrideSlot && !OverrideSlotName.IsEmpty())
		? OverrideSlotName
		: GetDefault<USaveGameServiceSettings>()->DefaultPlayInEditorSaveGameSlotName;
}
