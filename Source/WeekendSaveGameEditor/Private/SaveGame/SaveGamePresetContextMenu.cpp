///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGamePresetContextMenu.h"

#include "ContentBrowserMenuContexts.h"
#include "IAutomationControllerModule.h"
#include "ISessionFrontendModule.h"
#include "ToolMenus.h"
#include "AutomationTest/AutomationTestUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "SaveGame/SaveGamePreset.h"

#define LOCTEXT_NAMESPACE "WeekendSaveGameEditor"

namespace
{
	TArray<USaveGamePreset*> GetSelectedPresets(const UContentBrowserAssetContextMenuContext& Context)
	{
		TArray<USaveGamePreset*> Presets;
		for (const FAssetData& AssetData : Context.SelectedAssets)
		{
			if (USaveGamePreset* Preset = Cast<USaveGamePreset>(AssetData.GetAsset()))
			{
				Presets.Add(Preset);
			}
		}
		return Presets;
	}

	bool SelectionContainsPresets(const UContentBrowserAssetContextMenuContext& Context)
	{
		return Context.SelectedAssets.ContainsByPredicate([](const FAssetData& AssetData)
		{
			const UClass* AssetClass = AssetData.GetClass(EResolveClass::Yes);
			return AssetClass && AssetClass->IsChildOf(USaveGamePreset::StaticClass());
		});
	}

	void UpdateAnalysisOnPresets(const TArray<USaveGamePreset*>& Presets)
	{
		FScopedSlowTask SlowTask(Presets.Num(), LOCTEXT("UpdatingAnalysis", "Updating SaveGame Analysis..."));
		SlowTask.MakeDialog();

		for (USaveGamePreset* Preset : Presets)
		{
			SlowTask.EnterProgressFrame(1, FText::FromString(Preset->PresetName));
			Preset->UpdateAnalysis();
		}
	}

	void OpenTestsForPresets(const TArray<USaveGamePreset*>& Presets)
	{
		IAutomationControllerModule& AutomationModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>(TEXT("AutomationController"));
		const IAutomationControllerManagerRef Controller = AutomationModule.GetAutomationController();

		TArray<FString> AllTestNames;
		Controller->GetFilteredTestNames(OUT AllTestNames);

		TSet<FString> Suffixes;
		for (const USaveGamePreset* Preset : Presets)
		{
			Suffixes.Add(TEXT(".") + Preset->PresetName);
		}

		const TArray<FString> MatchingTests = AllTestNames.FilterByPredicate([&Suffixes](const FString& TestName)
		{
			for (const FString& Suffix : Suffixes)
			{
				if (TestName.EndsWith(Suffix))
					return true;
			}
			return false;
		});

		WeekendUtils::OpenTestsInAutomationTestFrontend(MatchingTests);
	}
}

void WeekendUtils::RegisterSaveGamePresetContextMenu()
{
	FToolMenuOwnerScoped OwnerScoped("WeekendSaveGameEditor");

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
	if (!Menu)
		return;

	FToolMenuSection& Section = Menu->FindOrAddSection("CommonAssetActions");
	Section.AddDynamicEntry("SaveGamePresetActions", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context || Context->SelectedAssets.IsEmpty())
			return;

		if (!SelectionContainsPresets(*Context))
			return;

		InSection.AddSubMenu(
			"WeekendUtils",
			LOCTEXT("WeekendUtils", "WeekendUtils"),
			LOCTEXT("WeekendUtilsTooltip", "Asset actions from the WeekendUtils plugin."),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				FToolMenuSection& SubSection = SubMenu->FindOrAddSection("SaveGamePresetActions");

				SubSection.AddMenuEntry(
					"UpdateSaveGameAnalysis",
					LOCTEXT("UpdateAnalysis", "Update Analysis"),
					LOCTEXT("UpdateAnalysisTooltip", "Re-runs all registered analyzers and updates the analysis report on the selected SaveGame presets."),
					FSlateIcon(),
					FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
					{
						const UContentBrowserAssetContextMenuContext* ActionContext = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>();
						if (!ActionContext)
							return;

						const TArray<USaveGamePreset*> Presets = GetSelectedPresets(*ActionContext);
						if (!Presets.IsEmpty())
						{
							UpdateAnalysisOnPresets(Presets);
						}
					})
				);

				SubSection.AddMenuEntry(
					"OpenSaveGameTests",
					LOCTEXT("OpenTests", "Open Tests"),
					LOCTEXT("OpenTestsTooltip", "Opens the Automation Frontend and selects all tests associated with the selected SaveGame presets."),
					FSlateIcon(),
					FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& MenuContext)
					{
						const UContentBrowserAssetContextMenuContext* ActionContext = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>();
						if (!ActionContext)
							return;

						const TArray<USaveGamePreset*> Presets = GetSelectedPresets(*ActionContext);
						if (!Presets.IsEmpty())
						{
							OpenTestsForPresets(Presets);
						}
					})
				);
			})
		);
	}));
}

void WeekendUtils::UnregisterSaveGamePresetContextMenu()
{
	UToolMenus::UnregisterOwner("WeekendSaveGameEditor");
}

#undef LOCTEXT_NAMESPACE
