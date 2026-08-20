/*
 * Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
 *
 * This file is part of the WeekendUtils UE5 Plugin.
 *
 * Distributed under the MIT License. See file: LICENSE.md
 * {@link https://github.com/barzb/UnrealWeekendUtils/blob/main/LICENSE}
 */

using UnrealBuildTool;

public class WeekendSaveGameEditor : ModuleRules
{
	public WeekendSaveGameEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"AssetTools",
				"AutomationController",
				"Blutility",
				"ContentBrowser",
				"DesktopPlatform",
				"SessionFrontend",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"WeekendUtils",
				"WeekendCheatMenu",
				"WeekendGameService",
				"WeekendSaveGame",
			}
		);
	}
}
