///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/RestorableGameServiceBase.h"

#include "SaveGame/SaveGameService.h"

FGameServiceUserConfig URestorableGameServiceBase::ConfigureGameServiceUser() const
{
	return Super::ConfigureGameServiceUser()
		.AddServiceDependency<USaveGameService>();
}

void URestorableGameServiceBase::BeginServiceStart()
{
	bIsWaitingForSaveGameRestore = true;
	SaveGameService = UseGameServiceAsWeakPtr<USaveGameService>();
	SaveGameService->OnBeforeSaved.AddUObject(this, &ThisClass::WriteToSaveGame);
	SaveGameService->OnAfterRestored.AddUObject(this, &ThisClass::HandleSaveGameLoaded);

	if (!SaveGameService->IsBusyLoading())
	{
		HandleSaveGameLoaded(SaveGameService->GetCurrentSaveGame());
	}
}

void URestorableGameServiceBase::BeginServiceShutdown(bool bIsWorldTearingDown)
{
	if (SaveGameService.IsValid())
	{
		SaveGameService->OnBeforeSaved.RemoveAll(this);
		SaveGameService->OnAfterRestored.RemoveAll(this);
		SaveGameService.Reset();
	}

	FinishServiceShutdown();
}

TOptional<FString> URestorableGameServiceBase::GetServiceStatusInfo() const
{
	if (bIsWaitingForSaveGameRestore)
	{
		return FString("Starting.. (Loading Save Game)");
	}

	return Super::GetServiceStatusInfo();
}

USaveGameService& URestorableGameServiceBase::GetSaveGameService() const
{
	checkf(SaveGameService.IsValid(), TEXT("Call only after this service has been started."));
	return *SaveGameService.Get();
}

void URestorableGameServiceBase::HandleSaveGameLoaded(const FCurrentSaveGame& SaveGame)
{
	if (IsServiceRunning())
	{
		RestoreFromSaveGame(SaveGame);
	}
	else
	{
		bIsWaitingForSaveGameRestore = false;
		StartRestorableService(SaveGame);
		FinishServiceStart();
	}

	if (!bCanRestoreMultipleSaveGames)
	{
		SaveGameService->OnAfterRestored.RemoveAll(this);
	}
}
