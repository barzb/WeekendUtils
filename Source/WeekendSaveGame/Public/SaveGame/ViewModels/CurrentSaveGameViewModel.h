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
#include "Utils/CommonAvailabilityEnum.h"

#include "CurrentSaveGameViewModel.generated.h"

class USaveGameService;
struct FCurrentSaveGame;

/**
 * ViewModel that represents the currently loaded SaveGame slot.
 */
UCLASS()
class WEEKENDSAVEGAME_API UCurrentSaveGameViewModel : public UMVVMViewModelBase,
													  public FGameServiceUser
{
	GENERATED_BODY()

public:
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bCanContinue = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bHasTimeOfLastSave = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	FDateTime UtcTimeOfLastSave = FDateTime();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void BeginUsage();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void EndUsage();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void ContinueSaveGame();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void CreateNewGame();

	UFUNCTION(FieldNotify, BlueprintCallable, Category = "Weekend Utils|Save Game")
	FTimespan GetTimespanSinceLastSave() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "Weekend Utils|Save Game", meta = (ExpandEnumAsExecs = "ReturnValue"))
	ECommonAvailability GetTimeSinceLastSave(int32& OutHours, int32& OutMinutes, int32& OutSeconds) const;

	// - FGameServiceUser
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const override;
	// - UObject
	virtual void BeginDestroy() override;
	// --

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void UpdateTimeSinceLastSave();

protected:
	UPROPERTY()
	TObjectPtr<USaveGameService> SaveGameService = nullptr;

	virtual void UpdateForCurrentSaveGame(const FCurrentSaveGame& CurrentSaveGame);
};
