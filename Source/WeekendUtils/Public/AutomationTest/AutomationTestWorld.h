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

#if WITH_AUTOMATION_WORKER

#include "Engine/EngineBaseTypes.h"
#include "Templates/SubclassOf.h"

#include "Tests/AutomationCommon.h"

class AGameModeBase;
class APlayerController;
class APlayerState;
class UGameInstance;
class ULocalPlayer;
class UWorld;
struct FURL;

namespace WeekendUtils
{
	/**
	 * Wrapper for a scoped world to be used in automation tests.
	 * World is created in the constructor and deleted in the destructor of the wrapper.
	 * Use @InitializeGame after creating the world to initialize and spawn the most important
	 * game framework classes (like game mode, player controller, ...).
	 */
	struct WEEKENDUTILS_API FScopedAutomationTestWorld
	{
	public:
		explicit FScopedAutomationTestWorld(FString InWorldName);
		virtual ~FScopedAutomationTestWorld();

		/** Optional config for @InitializeGame() */
		struct FConfig
		{
			FURL Url;
			TSubclassOf<APlayerState> PlayerStateClass;
		};

		/**
		 * Initialize and spawn the most important game framework classes for tests that require gamemode, player
		 * controller, etc. You do not have to call BeginPlay() before this, as it will be called internally.
		 */
		void InitializeGame();
		void InitializeGame(FConfig Config);

		FORCEINLINE UWorld* AsPtr() const { return World; }
		FORCEINLINE UWorld& AsRef() const { return *World; }

		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<AGameModeBase> GameMode = nullptr;
		TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;
		TObjectPtr<APlayerController> PlayerController = nullptr;
		TObjectPtr<UGameViewportClient> Viewport = nullptr;
	};
}

#endif