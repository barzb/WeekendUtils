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
#include "MVVMViewModelBase.h"

#include "SaveGameSlotViewModel.generated.h"

class USaveGame;
class USaveGameService;

/**
 * Base class of a SaveGame slot ViewModel that acts as list element of @USaveGameListViewModel.
 */
UCLASS(Abstract)
class WEEKENDSAVEGAME_API USaveGameSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	using FSlotName = FString;

	/** Events intended for @USaveGameListViewModel to listen to. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnSaveLoadRequested, const FSlotName&)
	FOnSaveLoadRequested OnLoadRequested;
	FOnSaveLoadRequested OnSaveRequested;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bCanBeSavedFromWidget = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bCanBeLoadedFromWidget = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsCurrentSaveGame = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsEmptySlot = false;

	virtual void BindToModel(const FSlotName& SlotName, USaveGameService& SaveGameService, bool bCanSave, bool bCanLoad);
	virtual void BindToSaveGame(const FSlotName& SlotName, const USaveGame& SaveGame) PURE_VIRTUAL(BindToSaveGame);
	virtual void BindToEmptySlot(const FSlotName& SlotName) PURE_VIRTUAL(BindToEmptySlot);
	virtual void UnbindFromModel() PURE_VIRTUAL(UnbindFromModel);
	virtual const USaveGame* FindSaveGameForSlot(USaveGameService& SaveGameService, const FSlotName& SlotName) const;

	FSlotName GetBoundSlotName() const { return BoundSlotName; }

protected:
	FSlotName BoundSlotName = FSlotName();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|SaveGame")
	bool TryLoadGameFromSlot();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|SaveGame")
	bool TrySaveGameToSlot();
};
