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
#include "UObject/ObjectPtr.h"

#include "CurrentSaveGame.generated.h"

class USaveGame;

/**
 * Container structure that provides various functionalities surrounding the
 * currently loaded SaveGame. Mainly used by @USaveGameService.
 */
USTRUCT()
struct WEEKENDSAVEGAME_API FCurrentSaveGame
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAfterRestored, const FCurrentSaveGame&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBeforeSaved, const FCurrentSaveGame&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAfterSaved, const FCurrentSaveGame&);

	using FSlotName = FString;

public:
	static FCurrentSaveGame CreateFromNewGame(USaveGame& SaveGame);
	static FCurrentSaveGame CreateFromLoadedGame(USaveGame& SaveGame, TOptional<FSlotName> LoadedFromSlotName = {});

	bool operator==(const FCurrentSaveGame& Other) const;
	bool operator!=(const FCurrentSaveGame& Other) const;
	bool operator==(const USaveGame& OtherSaveGame) const;
	bool operator!=(const USaveGame& OtherSaveGame) const;

	template <typename T>
	const T* GetPtr() const { return Cast<T>(SaveGameObject); }
	FORCEINLINE const USaveGame* GetPtr() const { return  SaveGameObject; }

	template <typename T>
	T* GetMutablePtr() const { return Cast<T>(SaveGameObject); }
	FORCEINLINE USaveGame* GetMutablePtr() const { return  SaveGameObject; }

	FORCEINLINE USaveGame& GetRef() const { return *SaveGameObject; }

	FORCEINLINE bool IsValid() const { return !!SaveGameObject; }

	FORCEINLINE bool WasEverLoaded() const { return bWasEverLoaded; }
	FORCEINLINE bool IsNewGame() const { return !bWasEverLoaded; }

	FORCEINLINE void UpdateTimeOfLastSave() { UtcTimeOfLastSave = FDateTime::UtcNow(); }
	FORCEINLINE TOptional<FDateTime> GetUtcTimeOfLastSave() const { return UtcTimeOfLastSave; }

	FORCEINLINE void SetSlotLastSavedTo(const FSlotName& SlotName) { SlotLastSavedTo = SlotName; }
	FORCEINLINE TOptional<FSlotName> GetSlotLastSavedTo() const { return SlotLastSavedTo; }

	FORCEINLINE void SetSlotLastRestoredFrom(const FSlotName& SlotName) { SlotLastRestoredFrom = SlotName; }
	FORCEINLINE TOptional<FSlotName> GetSlotLastRestoredFrom() const { return SlotLastRestoredFrom; }

	void Reset();

private:
	UPROPERTY()
	mutable TObjectPtr<USaveGame> SaveGameObject = nullptr;
	TOptional<FSlotName> SlotLastSavedTo = {};
	TOptional<FSlotName> SlotLastRestoredFrom = {};
	TOptional<FDateTime> UtcTimeOfLastSave = {};
	bool bWasEverLoaded = false;
};