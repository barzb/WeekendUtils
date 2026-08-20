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

#include "SaveGameFilePreset.generated.h"

class USaveGame;

/**
 * Concrete SaveGamePreset that stores the savegame as raw serialized bytes.
 * The savegame object is deserialized on demand from the byte array, exercising
 * the full serialization pipeline on every load. This makes it ideal for regression
 * testing: if a system change breaks deserialization, tests against FilePresets will catch it.
 *
 * Created by the SaveGameFileImportFactory when .sav files are dragged into the editor.
 * Not editable after creation since the raw byte data cannot be modified by hand.
 */
UCLASS(BlueprintType, CollapseCategories)
class WEEKENDSAVEGAME_API USaveGameFilePreset : public USaveGamePreset
{
	GENERATED_BODY()

public:
	// - USaveGamePreset
	virtual const USaveGame* GetPresetSaveGame() const override;
	virtual USaveGame* CreateSaveGameObject(USaveGameService& SaveGameService) const override;
	// --

	/** @returns the raw serialized savegame bytes stored in this preset. */
	const TArray<uint8>& GetSaveFileData() const { return SaveFileData; }

	/** Sets the raw serialized savegame bytes. Invalidates any cached deserialized object. The preset will take ownership of passed data. */
	void SetSaveFileData(TArray<uint8>&& InSaveFileData);

	// - UObject
	virtual void PostLoad() override;
	// --

protected:
	/** Raw serialized savegame data, stored as-is from the imported .sav file. */
	UPROPERTY()
	TArray<uint8> SaveFileData = {};

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "SaveGame", meta = (ForceUnits = "kb"))
	int32 FileSizeKb = 0;
#endif

private:
	/** Lazily deserialized savegame object. Transient: not saved with the asset. */
	UPROPERTY(Transient)
	mutable TObjectPtr<const USaveGame> CachedSaveGame = nullptr;

	USaveGame* DeserializeSaveGame(UObject* Outer) const;
};
