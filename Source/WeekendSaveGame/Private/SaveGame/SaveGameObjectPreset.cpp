///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGameObjectPreset.h"

#include "GameFramework/SaveGame.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameHeader.h"
#include "SaveGame/SaveGameService.h"

///////////////////////////////////////////////////////////////////////////////////////

USaveGameObjectPreset::USaveGameObjectPreset()
{
	SaveGame = CreateDefaultSubobject<UModularSaveGame>(TEXT("ModularSaveGame"));
}

const USaveGame* USaveGameObjectPreset::GetPresetSaveGame() const
{
	return SaveGame;
}

USaveGame* USaveGameObjectPreset::CreateSaveGameObject(USaveGameService& SaveGameService) const
{
	check(SaveGame);
	const FName SaveGameObjectName = MakeUniqueObjectName(&SaveGameService, SaveGame->GetClass(), FName("Preset"));
	USaveGame* DuplicatedSaveGame = DuplicateObject<USaveGame>(SaveGame, &SaveGameService, SaveGameObjectName);
	DuplicatedSaveGame->ClearFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_DefaultSubObject); // Clear CDO flags from duplication.

	UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(DuplicatedSaveGame);
	if (const FSimpleSaveGameHeaderData* SimpleHeaderData = HeaderData.GetPtr<FSimpleSaveGameHeaderData>(); ModularSaveGame && SimpleHeaderData)
	{
		ModularSaveGame->CreateHeaderData(*SimpleHeaderData);
	}

	return DuplicatedSaveGame;
}

void USaveGameObjectPreset::SetPresetSaveGame(const USaveGame& InSaveGame)
{
	SaveGame = &InSaveGame;
}
