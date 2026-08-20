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
#include "GameService/GameServiceUser.h"
#include "Templates/SubclassOf.h"

#include "SaveGameListViewModel.generated.h"

class USaveGamePreset;
class USaveLoadBehavior;
class USaveGameService;
class USaveGameSlotViewModel;

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Base class of a ViewModel that lists available SaveGame slots.
 * Works in accord with @USaveGameSlotViewModel.
 */
UCLASS(Abstract)
class WEEKENDSAVEGAME_API USaveGameListViewModel : public UMVVMViewModelBase,
												   public FGameServiceUser
{
	GENERATED_BODY()

public:
	using FSlotName = FString;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	TArray<TObjectPtr<USaveGameSlotViewModel>> Slots = {};

	// - FGameServiceUser
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const override;
	// --

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void BeginUsage(TSubclassOf<USaveGameSlotViewModel> SlotClass);

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void EndUsage();

protected:
	UPROPERTY()
	TSubclassOf<USaveGameSlotViewModel> SlotViewModelClass = nullptr;
	TWeakObjectPtr<USaveGameService> SaveGameService = nullptr;

	// - UObject
	virtual void BeginDestroy() override;
	// --

	virtual void Update();
	virtual TArray<FSlotName> GatherRelevantSlotNames() PURE_VIRTUAL(GatherRelevantSlotNames, return {};);
	virtual bool AllowsSavingFromWidget() const { return false; }
	virtual bool AllowsLoadingFromWidget() const { return false; }
	virtual bool HandleSaveRequestBySlot(const FSlotName& SlotName) { return false; }
	virtual bool HandleLoadRequestBySlot(const FSlotName& SlotName) { return false; }
};

///////////////////////////////////////////////////////////////////////////////////////

/** ViewModel implementation that lists SaveGame slots available for saving. */
UCLASS()
class WEEKENDSAVEGAME_API USaveGameSaveListViewModel : public USaveGameListViewModel
{
	GENERATED_BODY()

public:
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsSavingAllowed = true;

	// - USaveGameListViewModel
	virtual void BeginUsage(TSubclassOf<USaveGameSlotViewModel> SlotClass) override;
	// --

protected:
	// - USaveGameListViewModel
	virtual TArray<FSlotName> GatherRelevantSlotNames() override;
	virtual bool AllowsSavingFromWidget() const override { return true; }
	virtual bool HandleSaveRequestBySlot(const FSlotName& SlotName) override;
	// --
};

///////////////////////////////////////////////////////////////////////////////////////

/** ViewModel implementation that lists SaveGame slots available for loading. */
UCLASS()
class WEEKENDSAVEGAME_API USaveGameLoadListViewModel : public USaveGameListViewModel
{
	GENERATED_BODY()

protected:
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsLoadingAllowed = true;

	// - USaveGameListViewModel
	virtual void BeginUsage(TSubclassOf<USaveGameSlotViewModel> SlotClass) override;
	// --

	// - USaveGameListViewModel
	virtual TArray<FSlotName> GatherRelevantSlotNames() override;
	virtual bool AllowsLoadingFromWidget() const override { return true; }
	virtual bool HandleLoadRequestBySlot(const FSlotName& SlotName) override;
	// --
};

///////////////////////////////////////////////////////////////////////////////////////

/** ViewModel implementation that lists SaveGame presets available for loading. */
UCLASS()
class WEEKENDSAVEGAME_API USaveGamePresetListViewModel : public USaveGameListViewModel
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	TArray<TObjectPtr<const USaveGamePreset>> AvailablePresets = {};

	// - USaveGameListViewModel
	virtual void Update() override;
	virtual TArray<FSlotName> GatherRelevantSlotNames() override;
	virtual bool AllowsLoadingFromWidget() const override { return true; }
	virtual bool HandleLoadRequestBySlot(const FSlotName& SlotName) override;
	// --
};
