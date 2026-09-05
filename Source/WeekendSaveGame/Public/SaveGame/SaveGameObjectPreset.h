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
#include "SaveGame/SaveGamePreset.h"

#include "SaveGameObjectPreset.generated.h"

class USaveGame;

/**
 * Concrete SaveGamePreset that stores the savegame as an instanced UObject subobject.
 * The savegame data lives in the UObject hierarchy alongside the preset asset,
 * making it editable in the editor's details panel.
 *
 * Used for presets created manually via Content Browser or the SaveGameEditor feature.
 */
UCLASS(BlueprintType, CollapseCategories)
class WEEKENDSAVEGAME_API USaveGameObjectPreset : public USaveGamePreset
{
	GENERATED_BODY()

public:
	USaveGameObjectPreset();

	// - USaveGamePreset
	virtual const USaveGame* GetPresetSaveGame() const override;
	virtual USaveGame* CreateSaveGameObject(USaveGameService& SaveGameService) const override;
	// --

	void SetPresetSaveGame(const USaveGame& InSaveGame);

protected:
	/** The savegame object stored as an instanced subobject of this preset. */
	UPROPERTY(Instanced, EditDefaultsOnly, NoClear, Category = "SaveGame")
	TObjectPtr<const USaveGame> SaveGame;
};
