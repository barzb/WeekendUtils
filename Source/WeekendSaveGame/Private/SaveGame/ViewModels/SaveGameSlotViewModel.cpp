///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/ViewModels/SaveGameSlotViewModel.h"

#include "SaveGame/SaveGameService.h"

void USaveGameSlotViewModel::BindToModel(const FSlotName& SlotName, USaveGameService& SaveGameService, bool bCanSave, bool bCanLoad)
{
	BoundSlotName = SlotName;
	if (const USaveGame* SaveGame = FindSaveGameForSlot(SaveGameService, SlotName))
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsEmptySlot, false);
		BindToSaveGame(SlotName, *SaveGame);
	}
	else
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsEmptySlot, true);
		BindToEmptySlot(SlotName);
	}

	const FSlotName CurrentSlotName = SaveGameService.GetCurrentSaveGame().GetSlotLastRestoredFrom().Get("");
	UE_MVVM_SET_PROPERTY_VALUE(bIsCurrentSaveGame, (CurrentSlotName == SlotName));
	UE_MVVM_SET_PROPERTY_VALUE(bCanBeSavedFromWidget, bCanSave);
	UE_MVVM_SET_PROPERTY_VALUE(bCanBeLoadedFromWidget, bCanLoad);
}

const USaveGame* USaveGameSlotViewModel::FindSaveGameForSlot(USaveGameService& SaveGameService, const FSlotName& SlotName) const
{
	return SaveGameService.GetCachedSaveGameSnapshotAtSlot(SlotName);
}

bool USaveGameSlotViewModel::TryLoadGameFromSlot()
{
	return (OnLoadRequested.IsBound() && OnLoadRequested.Execute(BoundSlotName));
}

bool USaveGameSlotViewModel::TrySaveGameToSlot()
{
	ensureMsgf(!BoundSlotName.IsEmpty(), TEXT("SlotName should not be empty when trying to SaveGameToSlot"));
	return (OnSaveRequested.IsBound() && OnSaveRequested.Execute(BoundSlotName));
}
