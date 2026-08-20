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
#include "Components/SlateWrapperTypes.h"
#include "Containers/Ticker.h"
#include "GameService/GameServiceUser.h"
#include "SaveGame/SaveGameService.h"

#include "SaveLoadMarkerViewModel.generated.h"

/**
 * View Model for a marker widget showing when the game is being saved or loaded.
 * @BeginUsage and @EndUsage must be called manually by the owning view.
 */
UCLASS()
class WEEKENDSAVEGAME_API USaveLoadMarkerViewModel : public UMVVMViewModelBase,
													 public FGameServiceUser
{
	GENERATED_BODY()

public:
	// - FGameServiceUser
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const override;
	// --

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	ESlateVisibility SuggestedWidgetVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsSavingOrLoading = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsLoading = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SaveGame")
	bool bIsSaving = false;

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void BeginUsage();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	virtual void EndUsage();

	/** (Optional) Should be called before @BeginUsage() to configure how long the marker should be shown at minimum, even if save/load is faster. */
	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game")
	void ConfigureMinimumShowtime(float MinShowtimeSeconds = 2.f);

protected:
	TWeakObjectPtr<USaveGameService> SaveGameService = nullptr;
	TOptional<float> ConfiguredMinShowtimeSeconds = {};
	TOptional<FTSTicker::FDelegateHandle> MinShowtimeHandle = {};
	bool bIsShowingForMinShowtime = false;

	// - UObject
	virtual void BeginDestroy() override;
	// --

	virtual void UpdateForStatus(USaveGameService::EStatus NewStatus);
	virtual void StartMinShowTimer();
};
