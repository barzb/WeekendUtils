///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CheatMenuAction.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SVerticalBox;

DECLARE_DELEGATE_ThreeParams(FOnCheatExecuted, const ICheatMenuAction&, UWorld*, TArray<FString>);

class WEEKENDCHEATMENU_API SCheatMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCheatMenu) {}
		SLATE_ARGUMENT_DEFAULT(uint16, NumRecentlyUsedCheatsToShow) = 16;
		SLATE_EVENT(FOnCheatExecuted, OnCheatExecuted)
		SLATE_EVENT(FSimpleDelegate, OnCloseRequested)
	SLATE_END_ARGS()

	// - SCompoundWidget
	virtual ~SCheatMenu() override;
	virtual bool SupportsKeyboardFocus() const override;
	// --

	void Construct(const FArguments& InArgs);

protected:
	using EArgumentStyle = Cheats::EVariableStyle::Type;
	using FTabName = FName;
	using FSectionName = FName;

	struct FEntry
	{
		explicit FEntry(ICheatMenuAction* InCheatMenuAction, const FCheatMenuCategorySettings& InSettings);

		ICheatMenuAction* CheatMenuAction = nullptr;
		FCheatMenuCategorySettings Settings;
		TArray<TSharedPtr<FString>> Args;

		TArray<FString> GetArgs() const;
		const FString& GetCommandName() const;
		bool MatchesFilterText(const FString& TextToFilter) const;
		void ExecuteCheatMenuAction();
	};

	FTabName CurrentTabName = NAME_None;
	TArray<FString> FavoriteCheatMenuActions;
	TArray<FString> TextFilteredCheatMenuActions;
	TArray<FString> RecentlyUsedCheatMenuActions;
	uint16 NumRecentlyUsedCheatsToShow = 16;
	TOptional<FText> FilterText = {};

	TArray<TSharedPtr<FEntry>> Entries;
	TMap<FTabName, TArray<FSectionName>> SectionNamesInTabNames;

	TSharedPtr<SVerticalBox> TabList = nullptr;
	TSharedPtr<SWrapBox> SectionList = nullptr;
	TSharedPtr<STextBlock> ErrorText = nullptr;

	/**
	 * Combobox widgets tend to lose focus when they open, which can lead to them closing again when moving the cursor.
	 * To counter that, their OnComboBoxOpening method will reset the focus to the combobox button, and for that we need
	 * to cache the pointers to the combobox (indexed by a unique ID).
	 */
	TMap<FGuid, TWeakPtr<SComboBox<TSharedPtr<FString>>>> ComboBoxPointers;

	FOnCheatExecuted OnCheatExecuted;
	FSimpleDelegate OnCloseRequested;

	void CollectCheats();
	void PopulateTabList();
	void RefreshTabContent();
	void ConstructCommandsSection(const FSectionName& SectionName, const TArray<TSharedPtr<FEntry>>& CheatMenuActions);

	TArray<TSharedPtr<FEntry>> FilterCommands(const FTabName& TabName, const FSectionName& SectionName) const;
	TArray<TSharedPtr<FEntry>> FilterCommands(const TArray<FString>& CheatMenuActions) const;

	TSharedRef<SWidget> ConstructSection(const FSectionName& SectionName, TSharedRef<SWidget> Content);
	TSharedRef<SWidget> ConstructCheatMenuActionItems(const TArray<TSharedPtr<FEntry>>& CheatMenuActions);
	TSharedRef<SWidget> ConstructArgumentInput(const ICheatMenuAction::FArgumentInfo& ArgumentInfo, TSharedPtr<FString> InOutValue);

	void HandleFilterTextChanged(const FText& NewFilterText);
	void HandleCheatLogMessage(const ICheatMenuAction& CheatMenuAction, ELogVerbosity::Type Verbosity, const FString& Message);
	void HandleCheatExecuted(const ICheatMenuAction& CheatMenuAction, UWorld* World, TArray<FString> Args);
	FReply HandleCheatFavoriteButtonClicked(FString CheatName);

	void InsertCheatIntoRecentlyUsedList(const FString& CheatName);
	void ToggleCheatForFavorites(const FString& CheatName);

	void SaveFavoriteAndRecentlyUsedCheats() const;
	void RestoreFavoriteAndRecentlyUsedCheats();
};
