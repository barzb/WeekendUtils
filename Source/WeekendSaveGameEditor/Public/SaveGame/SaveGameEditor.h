///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"

#include "SaveGameEditor.generated.h"

class USaveGame;

/**
 * [Editor Only] Container object that can show exposed properties of a SaveGame object.
 */
UCLASS()
class WEEKENDSAVEGAMEEDITOR_API USaveGameEditor : public UObject
{
	GENERATED_BODY()

public:
	/** Opens the SaveGameEditor window for passed SaveGame object. */
	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game|Editor", meta = (DevelopmentOnly))
	static void OpenSaveGameEditor(const USaveGame* SaveGame);

	/** Opens the SaveGameEditor window for the currently active SaveGame object. */
	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game|Editor", meta = (DevelopmentOnly))
	static void OpenSaveGameEditorForCurrentSaveGame();

	// - UObject
	virtual bool IsEditorOnly() const override { return true; }
	// --

protected:
	/** Creates a new @USaveGamePreset data asset based on the currently edited SaveGame. */
	UFUNCTION(CallInEditor, Category = "Editor", meta = (DevelopmentOnly), DisplayName = "Convert To Preset")
	virtual void ConvertToPreset();

	/** Attempts to show the actively loaded SaveGame in this editor window. Game must be running. */
	UFUNCTION(CallInEditor, Category = "Editor", meta = (DevelopmentOnly), DisplayName = "Edit Current SaveGame")
	virtual void EditCurrentSaveGame();

	/** Attempts to load a compatible SaveGame file into this editor window. The loaded file does not affect any running sessions. */
	UFUNCTION(CallInEditor, Category = "Editor", meta = (DevelopmentOnly), DisplayName = "Edit SaveGame File")
	virtual void EditSaveGameFromFile();

	UFUNCTION(CallInEditor, Category = "Editor", meta = (DevelopmentOnly), DisplayName = "Analyze SaveGame")
	virtual void AnalyzeSaveGameComposition();

	virtual void SetSaveGame(const USaveGame* InSaveGame, TOptional<FString> OptionalInfo = {});

	UPROPERTY(VisibleAnywhere, Transient, Category = "Editor")
	FString EditorInfo = FString();

	UPROPERTY(Instanced, VisibleAnywhere, Transient, Category = "Save Game", meta = (ShowOnlyInnerProperties))
	TObjectPtr<const USaveGame> SaveGame = nullptr;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Save Game")
	FInstancedStruct HeaderData = FInstancedStruct();
};
