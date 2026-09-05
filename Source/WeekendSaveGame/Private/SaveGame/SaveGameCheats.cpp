///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "Cheat/CheatCommand.h"
#include "Cheat/CheatCommandCollection.h"
#include "GameService/GameServiceLocator.h"
#include "SaveGame/SaveGameService.h"

DEFINE_CHEAT_COLLECTION(WeekendSaveGameCheats, AsCheatMenuTab("Save/Load"))
{
	DEFINE_CHEAT_COMMAND(AutosaveCheat, "Cheat.SaveGame.Autosave")
	.DisplayAs("Autosave")
	DEFINE_CHEAT_EXECUTE(AutosaveCheat)
	{
		USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(GetWorld());
		if (LogInvalidity(SaveGameService, "SaveGameService not available"))
			return;

		SaveGameService->RequestAutosave("Cheat.SaveGame.Autosave");
	}

	DEFINE_CHEAT_COMMAND(LoadAutosaveCheat, "Cheat.SaveGame.LoadAutosave")
	.DisplayAs("Load Autosave")
	DEFINE_CHEAT_EXECUTE(LoadAutosaveCheat)
	{
		USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(GetWorld());
		if (LogInvalidity(SaveGameService, "SaveGameService not available"))
			return;

		SaveGameService->RequestLoadCurrentSaveGameFromSlot("Cheat.SaveGame.LoadAutosave", SaveGameService->GetAutosaveSlotName());
	}
}
