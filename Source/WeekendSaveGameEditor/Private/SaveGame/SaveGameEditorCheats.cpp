///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#include "Cheat/CheatCommand.h"
#include "Cheat/CheatCommandCollection.h"
#include "GameService/GameServiceLocator.h"
#include "SaveGame/SaveGameEditor.h"
#include "SaveGame/SaveGameService.h"

DEFINE_CHEAT_COLLECTION(WeekendSaveGameEditorCheats, AsCheatMenuTab("Save/Load"))
{
	DEFINE_CHEAT_COMMAND(OpenSaveGameEditorCheat, "Cheat.SaveGame.OpenEditor")
	.DisplayAs("Open SaveGame Editor")
	DEFINE_CHEAT_EXECUTE(OpenSaveGameEditorCheat)
	{
		const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(GetWorld());
		if (LogInvalidity(SaveGameService, "SaveGameService not available"))
			return;

		const FCurrentSaveGame& CurrentSaveGame = SaveGameService->GetCurrentSaveGame();
		USaveGameEditor::OpenSaveGameEditor(CurrentSaveGame.GetPtr());
	}
}
