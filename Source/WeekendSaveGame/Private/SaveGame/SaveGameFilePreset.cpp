///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGameFilePreset.h"

#include "GameFramework/SaveGame.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameHeader.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogSaveGameFilePreset, Log, All);

///////////////////////////////////////////////////////////////////////////////////////

const USaveGame* USaveGameFilePreset::GetPresetSaveGame() const
{
	if (!CachedSaveGame && SaveFileData.Num() > 0)
	{
		CachedSaveGame = DeserializeSaveGame(const_cast<USaveGameFilePreset*>(this));
	}

	return CachedSaveGame;
}

USaveGame* USaveGameFilePreset::CreateSaveGameObject(USaveGameService& SaveGameService) const
{
	USaveGame* NewSaveGame = DeserializeSaveGame(&SaveGameService);
	if (!IsValid(NewSaveGame))
	{
		UE_LOG(LogSaveGameFilePreset, Error, TEXT("Failed to deserialize savegame from file data in preset '%s'."), *PresetName);
		return nullptr;
	}

	if (UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(NewSaveGame); IsValid(ModularSaveGame))
	{
		ModularSaveGame->SetInstancedHeaderData(CopyTemp(HeaderData));
	}

	return NewSaveGame;
}

void USaveGameFilePreset::SetSaveFileData(TArray<uint8>&& InSaveFileData)
{
	SaveFileData = MoveTemp(InSaveFileData);
#if WITH_EDITORONLY_DATA
	FileSizeKb = (SaveFileData.Num() / 1024);
#endif
	CachedSaveGame = nullptr;
}

void USaveGameFilePreset::PostLoad()
{
	Super::PostLoad();

	if (SaveFileData.Num() > 0 && !CachedSaveGame)
	{
		CachedSaveGame = DeserializeSaveGame(this);
		if (!CachedSaveGame)
		{
			UE_LOG(LogSaveGameFilePreset, Warning, TEXT("Failed to deserialize cached savegame for preset '%s' during PostLoad."), *PresetName);
		}
	}
}

USaveGame* USaveGameFilePreset::DeserializeSaveGame(UObject* Ouer) const
{
	if (SaveFileData.IsEmpty())
		return nullptr;

	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
	const USaveLoadBehavior* SaveLoadBehavior = GetDefault<USaveLoadBehavior>(Settings->SaveLoadBehavior.LoadSynchronous());
	const USaveGameSerializer* Serializer = IsValid(SaveLoadBehavior)
		? GetDefault<USaveGameSerializer>(SaveLoadBehavior->GetSaveGameSerializerClass())
		: GetDefault<UModularSaveGameSerializer>();

	USaveGame* Result = nullptr;
	if (!Serializer->TryDeserializeSaveGame(SaveFileData, OUT Result))
		return nullptr;

	return Result;
}
