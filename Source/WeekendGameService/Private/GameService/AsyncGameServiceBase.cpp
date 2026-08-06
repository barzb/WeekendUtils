///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/AsyncGameServiceBase.h"

void UAsyncGameServiceBase::StartService()
{
	ensure(CurrentStatus == EAsyncServiceStatus::Inactive);
	CurrentStatus = EAsyncServiceStatus::Starting;

	AttemptToStartService();
}

void UAsyncGameServiceBase::AttemptToStartService()
{
	bIsWaitingForDependencies = (bWaitForDependenciesBeforeStarting && !AreAllDependenciesReady());
	bIsWaitingForWorldToBeginPlay = (bWaitForWorldToBeginPlay && !GetWorld()->HasBegunPlay());
	const bool bIsReadyToStart = (!bIsWaitingForDependencies && !bIsWaitingForWorldToBeginPlay);

	if (bIsReadyToStart)
	{
		if (WaitForWorldDelegateHandle.IsValid())
		{
			// Clear potential delegate binding when waiting for the world to begin play:
			GetWorld()->OnWorldBeginPlay.Remove(WaitForWorldDelegateHandle);
			WaitForWorldDelegateHandle.Reset();
		}

		BeginServiceStart();
		return;
	}

	if (bIsWaitingForDependencies)
	{
		WaitForDependencies(FOnWaitingFinished::CreateUObject(this, &UAsyncGameServiceBase::AttemptToStartService));
	}

	if (bIsWaitingForWorldToBeginPlay)
	{
		WaitForWorldToBeginPlay(*GetWorld());
	}
}

void UAsyncGameServiceBase::WaitForWorldToBeginPlay(UWorld& World)
{
	if (World.HasBegunPlay())
	{
		AttemptToStartService();
		return;
	}

	WaitForWorldDelegateHandle = World.OnWorldBeginPlay.AddUObject(this, &UAsyncGameServiceBase::AttemptToStartService);
}

void UAsyncGameServiceBase::ShutdownService()
{
	StopWaitingForDependencies();

	UWorld* World = GetWorld();
	if (WaitForWorldDelegateHandle.IsValid())
	{
		if (IsValid(World))
		{
			World->OnWorldBeginPlay.Remove(WaitForWorldDelegateHandle);
		}
		WaitForWorldDelegateHandle.Reset();
	}

	const bool bIsWorldTearingDown = (!IsValid(World) || World->bIsTearingDown);
	if (!bIsWorldTearingDown)
	{
		AddToRoot(); // Make sure service is not garbage collected while shutting down.
	}

	CurrentStatus = EAsyncServiceStatus::Stopping;
	BeginServiceShutdown(bIsWorldTearingDown);
}

TOptional<FString> UAsyncGameServiceBase::GetServiceStatusInfo() const
{
	switch (CurrentStatus)
	{
		case EAsyncServiceStatus::Inactive:
			return FString("Inactive");

		case EAsyncServiceStatus::Starting:
		{
			if (bIsWaitingForDependencies)
				return FString("Starting.. (Waiting for dependencies)");
			if (bIsWaitingForWorldToBeginPlay)
				return FString("Starting.. (Waiting for world to begin play)");

			return FString("Starting..");
		}

		case EAsyncServiceStatus::Stopping:
			return FString("Stopping..");

		case EAsyncServiceStatus::Running:
		default: return {};
	}
}

void UAsyncGameServiceBase::FinishServiceStart()
{
	CurrentStatus = EAsyncServiceStatus::Running;

	while (PendingServiceStartCallbacks.Num() > 0)
	{
		PendingServiceStartCallbacks.Pop().ExecuteIfBound();
	}
}

void UAsyncGameServiceBase::FinishServiceShutdown()
{
	CurrentStatus = EAsyncServiceStatus::Inactive;
	RemoveFromRoot(); // Free up for GC again.
}

void UAsyncGameServiceBase::WaitUntilServiceIsRunning(FOnAsyncGameServiceStarted Callback)
{
	if (IsServiceRunning())
	{
		Callback.ExecuteIfBound();
	}
	else
	{
		PendingServiceStartCallbacks.Add(Callback);
	}
}
