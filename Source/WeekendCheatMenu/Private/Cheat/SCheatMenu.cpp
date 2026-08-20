///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "Cheat/SCheatMenu.h"

#include "Cheat/CheatCommand.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Internationalization/BreakIterator.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/StyleColors.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

///////////////////////////////////////////////////////////////////////////////////////
/// Cheats:

DEFINE_CHEAT_COLLECTION(CheatMenuCheats)
{
	DEFINE_CHEAT_COMMAND(OpenCheatMenuCheat, "Cheat.OpenCheatMenu")
	.DescribeCheat("Opens the cheat menu in a new window")
	DEFINE_CHEAT_EXECUTE(OpenCheatMenuCheat)
	{
		static TSharedPtr<SWindow> SlateWindow = nullptr;

		// Window already open somewhere:
		if (SlateWindow.IsValid() && SlateWindow->IsActive())
		{
			SlateWindow->BringToFront();
			return;
		}

		// Create new window:
		SlateWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.ClientSize(FVector2d(1280.f, 720.f))
		.CreateTitleBar(true)
		.HasCloseButton(true)
		.IsInitiallyMaximized(false)
		.MinHeight(64.f)
		.MinWidth(128.f)
		.SaneWindowPlacement(true)
		.SizingRule(ESizingRule::UserSized)
		.Style(&FCoreStyle::Get().GetWidgetStyle<FWindowStyle>("Window"))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.Title(INVTEXT("Cheat Menu"))
		.FocusWhenFirstShown(true)
		.UseOSWindowBorder(false);
		SlateWindow->SetContent(SNew(SCheatMenu));

		// Open the window:
		FSlateApplication& SlateApp = FSlateApplication::Get();
		SlateApp.AddWindow(SlateWindow.ToSharedRef());
		SlateApp.GetRenderer()->CreateViewport(SlateWindow.ToSharedRef());
		SlateApp.OnPreShutdown().AddLambda([]
		{
			if (SlateWindow.IsValid())
			{
				FSlateApplication::Get().RequestDestroyWindow(SlateWindow.ToSharedRef());
				SlateWindow.Reset();
			}
		});
	}
}

///////////////////////////////////////////////////////////////////////////////////////
/// Internal Helpers:

namespace
{
	const FName TAB_NAME_NONE = FName();
	const FName SECTION_NAME_NONE = FName();
	const FName FAVORITE_TAB_NAME = FName("Favorites");
	const FName RECENTLY_USED_TAB_NAME = FName("RecentlyUsed");
	const FName FILTER_RESULT_TAB_NAME = FName("Filtered");
	const FName DEFAULT_TAB_NAME = FAVORITE_TAB_NAME;

	const FString CHEAT_MENU_INI_FILE = GGameUserSettingsIni;
	const TCHAR* CHEAT_MENU_INI_SECTION = TEXT("WeekendUtils.CheatMenu");
	const TCHAR* CHEAT_MENU_INI_FAVORITES = TEXT("FavoritedCheats");
	const TCHAR* CHEAT_MENU_INI_RECENTLY_USED = TEXT("RecentlyUsedCheats");

	const FMargin DEFAULT_PADDING = FMargin(5.f);

	FSlateFontInfo GetDefaultCheatMenuTextFont() { return FCoreStyle::GetDefaultFontStyle("Regular", 8); }

	/**
	 * Combobox widgets tend to lose focus when they open, which can lead to them closing again when moving the cursor.
	 * To counter that, their OnComboBoxOpening method will reset the focus to the combobox button, and for that we need
	 * to cache the pointers to the combobox (indexed by a unique ID).
	 */
	TMap<FGuid, TWeakPtr<SComboBox<TSharedPtr<FString>>>> CheatMenuComboBoxPointers;

	struct FSortMenuEntryPredicate
	{
		FORCEINLINE bool operator()(const FName& A, const FName& B) const
		{
			return (A.IsNone() || A.Compare(B) < 0);
		}
	};

	UWorld* FindPlayWorld()
	{
		// Determine the target world at the moment of execution,
		// because the cheat menu might've been created in another level or even the editor
		// before PIE, so it can't know which world is "current".
		// The following approach is also how the engine does it for console commands:
		UWorld* TargetWorld = nullptr;
		if (const ULocalPlayer* Player = GEngine->GetDebugLocalPlayer(); IsValid(Player))
		{
			TargetWorld = Player->GetWorld();
		}
		return GEngine->GetCurrentPlayWorld(TargetWorld);
	}
}

///////////////////////////////////////////////////////////////////////////////////////
/// SCheatMenu:

void SCheatMenu::Construct(const FArguments& InArgs)
{
	CurrentTabName = DEFAULT_TAB_NAME;
	NumRecentlyUsedCheatsToShow = InArgs._NumRecentlyUsedCheatsToShow;
	OnCheatExecuted = InArgs._OnCheatExecuted;
	OnCloseRequested = InArgs._OnCloseRequested;

	bCanSupportFocus = true;

	TSharedRef<SHorizontalBox> MainContent = SNew(SHorizontalBox);

	// [Left] Tab List:
	MainContent->AddSlot()
	.AutoWidth()
	.Padding(DEFAULT_PADDING)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SScrollBox)
		.ScrollBarAlwaysVisible(true)
		.ScrollBarThickness(FVector2d(5.f))

		+SScrollBox::Slot()
		.Padding(DEFAULT_PADDING)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(TabList, SVerticalBox)
		]
	];

	if (InArgs._OnCloseRequested.IsBound())
	{
		// [Left|Top] Close Button:
		TabList->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.f, 0.f, 0.f, 5.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.ToolTipText(INVTEXT("Close Cheat Menu"))
			.ButtonStyle(FAppStyle::Get(), "Window.CloseButtonHover")
			.OnClicked_Lambda([this]()
			{
				OnCloseRequested.ExecuteIfBound();
				return FReply::Handled();
			})
			.ContentPadding(4.0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.X"))
			]
		];
	}

	// [Left|Top] Search Box:
	TabList->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 5.f)
	[
		SNew(SSearchBox)
		.ToolTipText(INVTEXT("Filter Cheats"))
		.MinDesiredWidth(96.f)
		.DelayChangeNotificationsWhileTyping(true)
		.OnTextChanged(this, &SCheatMenu::HandleFilterTextChanged)
		.OnTextCommitted_Lambda([this](const FText& NewFilterText, ETextCommit::Type){ HandleFilterTextChanged(NewFilterText); })
	];

	// [Right] Current Tab Content:
	MainContent->AddSlot()
	.FillWidth(1.f)
	.Padding(DEFAULT_PADDING)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)

		// [Right|Top] Content:
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SScrollBox)
			+SScrollBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(SectionList, SWrapBox)
				.HAlign(HAlign_Fill)
				.Orientation(Orient_Horizontal)
			]
		]

		// [Right|Bottom] Error Bar:
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SNew(SOverlay)

			// Background:
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			]

			// Output Box:
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(5.f)
			[
				SNew(SHorizontalBox)

				// Text:
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ErrorText, STextBlock)
					.Justification(ETextJustify::Left)
				]

				// Clear Button:
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.ToolTipText(INVTEXT("Clear Output"))
					.IsEnabled_Lambda([this]() -> bool { return !ErrorText->GetText().IsEmpty(); })
					.OnClicked_Lambda([this]()
					{
						ErrorText->SetText(FText::GetEmpty());
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
					]
				]
			]
		]
	];

	// Populate:
	CollectCheats();
	PopulateTabList();
	RefreshTabContent();

	// Add the sub-widgets to the ChildSlot of this compound widget:
	ChildSlot
	[
		MainContent
	];
}

SCheatMenu::FEntry::FEntry(ICheatMenuAction* InCheatMenuAction, const FCheatMenuCategorySettings& InSettings) :
	CheatMenuAction(InCheatMenuAction), Settings(InSettings)
{
	const int32 NumArgs = CheatMenuAction->GetArgumentsInfo().Num();
	TArray<FString> ArgValues;

	// Try to load arg values from config ini, if they have already been executed:
	if (GConfig != nullptr)
	{
		//FString ArgValuesFromCfg;
		GConfig->GetSingleLineArray(CHEAT_MENU_INI_SECTION, *GetCommandName(), OUT ArgValues, CHEAT_MENU_INI_FILE);
		//ArgValuesFromCfg.ParseIntoArray(OUT ArgValues, TEXT(","));
	}

	// Make sure the value array size is in sync with the actual args array:
	ArgValues.SetNum(NumArgs);

	// Construct the args with possibly some default values:
	Args.Empty(NumArgs);
	for (const FString& ArgValue : ArgValues)
	{
		Args.Add(MakeShared<FString>(ArgValue));
	}
}

SCheatMenu::~SCheatMenu()
{
	for (const TSharedPtr<FEntry>& Entry : Entries)
	{
		Entry->CheatMenuAction->OnLogMessage.RemoveAll(this);
		Entry->CheatMenuAction->OnAfterExecuted.RemoveAll(this);
	}
	Entries.Empty();
	TabList.Reset();
	SectionList.Reset();
	ErrorText.Reset();
	CheatMenuComboBoxPointers.Reset();
}

bool SCheatMenu::SupportsKeyboardFocus() const
{
	return true;
}

TArray<FString> SCheatMenu::FEntry::GetArgs() const
{
	TArray<FString> Result;
	Algo::Transform(Args, OUT Result, [](const TSharedPtr<FString>& Elem){ return *Elem.Get(); });
	return Result;
}

const FString& SCheatMenu::FEntry::GetCommandName() const
{
	return CheatMenuAction->GetName();
}

bool SCheatMenu::FEntry::MatchesFilterText(const FString& TextToFilter) const
{
	return CheatMenuAction->GetName().Contains(TextToFilter)
		|| CheatMenuAction->GetDisplayName().Contains(TextToFilter);
}

void SCheatMenu::FEntry::ExecuteCheatMenuAction()
{
	CheatMenuAction->ExecuteWithArgs(GetArgs(), FindPlayWorld());

	// Save the args to the config ini, so they can be restored the next time the cheat menu opens:
	if (GConfig != nullptr)
	{
		GConfig->SetSingleLineArray(CHEAT_MENU_INI_SECTION, *GetCommandName(), GetArgs(), CHEAT_MENU_INI_FILE);
		GConfig->Flush(false, CHEAT_MENU_INI_FILE);
	}
}

void SCheatMenu::CollectCheats()
{
	// Clear previous:
	for (const TSharedPtr<FEntry>& Entry : Entries)
	{
		Entry->CheatMenuAction->OnLogMessage.RemoveAll(this);
		Entry->CheatMenuAction->OnAfterExecuted.RemoveAll(this);
	}
	Entries.Empty();
	SectionNamesInTabNames.Empty();

	// Collect anew:
	for (const Cheats::FCheatCommandCollection* Collection : Cheats::GetAllCollections())
	{
		if (!Collection->ShowInCheatMenu())
			continue;

		const FCheatMenuCategorySettings& Settings = Collection->GetCheatMenuSettings();
		const FName& Section = Settings.MenuTabName.Get(TAB_NAME_NONE);
		const FName& Tab = Settings.MenuSectionName.Get(SECTION_NAME_NONE);
		SectionNamesInTabNames.FindOrAdd(Section).AddUnique(Tab);

		for (ICheatMenuAction* CheatMenuAction : Collection->GetRegisteredCheatMenuActions())
		{
			Entries.Add(MakeShared<FEntry>(CheatMenuAction, Settings));
			CheatMenuAction->OnLogMessage.AddSP(this, &SCheatMenu::HandleCheatLogMessage);
			CheatMenuAction->OnAfterExecuted.AddSP(this, &SCheatMenu::HandleCheatExecuted);
		}
	}

	// Sort tabs and sections by name:
	SectionNamesInTabNames.KeySort(FSortMenuEntryPredicate());
	for (TTuple<FTabName, TArray<FSectionName>>& Pair : SectionNamesInTabNames)
	{
		Pair.Value.Sort(FSortMenuEntryPredicate());
	}

	RestoreFavoriteAndRecentlyUsedCheats();
}

TArray<TSharedPtr<SCheatMenu::FEntry>> SCheatMenu::FilterCommands(const FTabName& TabName, const FSectionName& SectionName) const
{
	return Entries.FilterByPredicate([TabName, SectionName](const TSharedPtr<FEntry>& Entry)
	{
		return Entry->Settings.MenuTabName.Get(TAB_NAME_NONE).IsEqual(TabName)
			&& Entry->Settings.MenuSectionName.Get(SECTION_NAME_NONE).IsEqual(SectionName);
	});
}

TArray<TSharedPtr<SCheatMenu::FEntry>> SCheatMenu::FilterCommands(const TArray<FString>& CheatMenuActions) const
{
	TArray<TSharedPtr<FEntry>> Result = Entries.FilterByPredicate([&CheatMenuActions](const TSharedPtr<FEntry>& Entry)
	{
		return CheatMenuActions.Contains(Entry->GetCommandName());
	});
	Result.Sort([&CheatMenuActions](const TSharedPtr<FEntry>& Lhs, const TSharedPtr<FEntry>& Rhs)
	{
		return CheatMenuActions.IndexOfByKey(Lhs->GetCommandName()) < CheatMenuActions.IndexOfByKey(Rhs->GetCommandName());
	});
	return Result;
}

void SCheatMenu::PopulateTabList()
{
	struct FTabProperties
	{
		explicit FTabProperties(FName InTabName) :
			TabName(InTabName), Text(FText::Format(INVTEXT("📃 {0}"), FText::FromName(InTabName))) {}
		explicit FTabProperties(FName InTabName,const FText& InText, const FText& InToolTip) :
			TabName(InTabName), Text(InText), ToolTip(InToolTip) {}

		FName TabName;
		FText Text;
		FText ToolTip;
	};

	TArray<FTabProperties> Tabs;
	Algo::Transform(SectionNamesInTabNames, OUT Tabs, [](const auto& Itr){ return FTabProperties(Itr.Key); });
	Tabs.Insert(FTabProperties(FAVORITE_TAB_NAME, INVTEXT("⭐ Favorites"), INVTEXT("Favorited Cheats")), 0);
	Tabs.Insert(FTabProperties(RECENTLY_USED_TAB_NAME, INVTEXT("🕙 Recently Used"), INVTEXT("Recently Used Cheats")), 1);

	for (const FTabProperties& Tab : Tabs)
	{
		TabList->AddSlot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonColorAndOpacity_Lambda([this, TabName = Tab.TabName]()
			{
				return (CurrentTabName == TabName ? FStyleColors::Black : FStyleColors::White);
			})
			.OnClicked_Lambda([this, Tab]() -> FReply
			{
				FilterText.Reset();
				CurrentTabName = Tab.TabName;
				RefreshTabContent();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Left)
				.Text(FText::Format(INVTEXT("{0}    "), Tab.Text)) // Hack to compensate width offset from scroll bar.
				.Font(GetDefaultCheatMenuTextFont())
				.ToolTipText(Tab.ToolTip)
			]
		];
	}
}

void SCheatMenu::RefreshTabContent()
{
	CheatMenuComboBoxPointers.Reset();
	SectionList->ClearChildren();
	if (!CurrentTabName.IsValid())
		return;

	if (SectionNamesInTabNames.Contains(CurrentTabName))
	{
		for (const FSectionName& SectionName : SectionNamesInTabNames[CurrentTabName])
		{
			ConstructCommandsSection(SectionName, FilterCommands(CurrentTabName, SectionName));
		}
	}
	else if (CurrentTabName == FAVORITE_TAB_NAME)
	{
		ConstructCommandsSection(NAME_None, FilterCommands(FavoriteCheatMenuActions));
	}
	else if (CurrentTabName == RECENTLY_USED_TAB_NAME)
	{
		ConstructCommandsSection(NAME_None, FilterCommands(RecentlyUsedCheatMenuActions));
	}
	else if (CurrentTabName == FILTER_RESULT_TAB_NAME)
	{
		ConstructCommandsSection(NAME_None, FilterCommands(TextFilteredCheatMenuActions));
	}
}

void SCheatMenu::ConstructCommandsSection(const FSectionName& SectionName, const TArray<TSharedPtr<FEntry>>& CheatMenuActions)
{
	SectionList->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.FillEmptySpace(true)
	[
		ConstructSection(SectionName, ConstructCheatMenuActionItems(CheatMenuActions))
	];
}

TSharedRef<SWidget> SCheatMenu::ConstructSection(const FSectionName& SectionName, TSharedRef<SWidget> Content)
{
	TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);

	// Section Header:
	if (!SectionName.IsNone())
	{
		Section->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.AutoHeight()
		[
			SNew(SOverlay)

			// Background:
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			]

			// Header:
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(5.f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.Text(FText::FromString(SectionName.ToString()))
				.Font(GetDefaultCheatMenuTextFont())
				.ColorAndOpacity(EStyleColor::AccentWhite)
				.Justification(ETextJustify::Center)
				.Margin(FMargin(10.f, -2.f))
			]
		];
	}

	// Section Content:
	Section->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.AutoHeight()
	[
		SNew(SOverlay)

		// Background:
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		]

		// Commands List:
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(5.f)
		[
			Content
		]
	];

	return SNew(SBox)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.MinDesiredWidth(256.f)
	.Padding(DEFAULT_PADDING)
	[
		Section
	];
}

TSharedRef<SWidget> SCheatMenu::ConstructCheatMenuActionItems(const TArray<TSharedPtr<FEntry>>& CheatMenuActions)
{
	TSharedRef<SWrapBox> OuterWrapBox =
		SNew(SWrapBox)
		.HAlign(HAlign_Left)
		.Orientation(Orient_Horizontal)
		.InnerSlotPadding(FVector2d(10.f))
		.UseAllottedSize(true);

	for (auto Itr = CheatMenuActions.CreateConstIterator(); Itr; ++Itr)
	{
		TSharedPtr<FEntry> Entry = *Itr;
		const ICheatMenuAction* CheatMenuAction = Entry->CheatMenuAction;
		const FString CheatName = CheatMenuAction->GetName();
		TSharedPtr<SVerticalBox> CommandBox;

		OuterWrapBox->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.Padding(DEFAULT_PADDING)
		[
			SAssignNew(CommandBox, SVerticalBox)

			// Execution Button + Fav Icon:
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				// Favorite Star:
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.ContentPadding(0.f)
					.ToolTipText_Lambda([this, CheatName]()
					{
						return FavoriteCheatMenuActions.Contains(CheatName) ?
							INVTEXT("Remove from favorites") : INVTEXT("Add to favorites");
					})
					.OnClicked(this, &SCheatMenu::HandleCheatFavoriteButtonClicked, CheatName)
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Font(FAppStyle::Get().GetFontStyle("NormalBold"))
						.ColorAndOpacity_Lambda([this, CheatName]()
						{
							return FavoriteCheatMenuActions.Contains(CheatName) ?
								EStyleColor::AccentYellow : EStyleColor::AccentGray;
						})
						.Text_Lambda([this, CheatName]()
						{
							return FavoriteCheatMenuActions.Contains(CheatName) ?
								INVTEXT("★") : INVTEXT("☆");
						})
					]
				]

				// Execution Button:
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.HAlign(HAlign_Fill)
				[
					SNew(SButton)
					.OnClicked_Lambda([Entry]() -> FReply
					{
						Entry->ExecuteCheatMenuAction();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(FText::FromString(CheatMenuAction->GetDisplayName()))
						.Font(GetDefaultCheatMenuTextFont())
						.ToolTipText(FText::FromString(CheatMenuAction->GetCommandInfo()))
						.HighlightText_Lambda([this](){ return FilterText.Get(FText()); })
					]
				]
			]
		];

		// Arguments:
		auto ArgValueItr = Entry->Args.CreateIterator();
		for (const ICheatMenuAction::FArgumentInfo& ArgumentInfo : CheatMenuAction->GetArgumentsInfo())
		{
			TSharedPtr<FString>& ArgValue = *ArgValueItr; ++ArgValueItr;
			const bool bIsText = ArgumentInfo.IsTextArgument();

			CommandBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)

				// Argument Name:
				+ SHorizontalBox::Slot()
				.FillWidth(3.f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ArgumentInfo.Name + ": "))
					.Font(GetDefaultCheatMenuTextFont())
					.Justification(ETextJustify::Right)
					.AutoWrapText(true)
					.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
					.ToolTipText(FText::FromString(ArgumentInfo.Description))
				]

				// Argument Input:
				+ SHorizontalBox::Slot()
				.FillWidth(bIsText ? 5.f : 2.f)
				.MaxWidth(bIsText ? 256.f : 64.f)
				.HAlign(bIsText ? HAlign_Fill : HAlign_Left)
				.VAlign(VAlign_Center)
				[
					ConstructArgumentInput(ArgumentInfo, IN OUT ArgValue)
				]
			];
		}
	}
	return OuterWrapBox;
}

TSharedRef<SWidget> SCheatMenu::ConstructArgumentInput(const ICheatMenuAction::FArgumentInfo& ArgumentInfo, TSharedPtr<FString> InOutValue)
{
	static constexpr float MinDesiredWith = 64.f;
	switch (ArgumentInfo.Style)
	{
		case EArgumentStyle::Number:
		{
			int32 DefaultValue = 0;
			LexFromString(OUT DefaultValue, **InOutValue);
			*InOutValue = FString::FromInt(DefaultValue);
			return SNew(SSpinBox<int32>)
			.Delta(1)
			.MinDesiredWidth(MinDesiredWith)
			.ToolTipText(FText::FromString(ArgumentInfo.Description))
			.Font(GetDefaultCheatMenuTextFont())
			.Value(DefaultValue)
			.OnValueCommitted_Lambda([InOutValue](int32 Value, ETextCommit::Type) { *InOutValue = FString::FromInt(Value); });
		}
		case EArgumentStyle::FloatNumber:
		{
			float DefaultValue = 0.f;
			LexFromString(OUT DefaultValue, **InOutValue);
			*InOutValue = LexToString(DefaultValue);
			return SNew(SSpinBox<float>)
			.Delta(0.1f)
			.MinDesiredWidth(MinDesiredWith)
			.ToolTipText(FText::FromString(ArgumentInfo.Description))
			.Font(GetDefaultCheatMenuTextFont())
			.Value(DefaultValue)
			.OnValueCommitted_Lambda([InOutValue](float Value, ETextCommit::Type) { *InOutValue = LexToString(Value); });
		}
		case EArgumentStyle::TrueFalse:
		{
			bool bDefaultValue = false;
			LexFromString(OUT bDefaultValue, **InOutValue);
			*InOutValue = LexToString(bDefaultValue);
			return SNew(SCheckBox)
			.ToolTipText(FText::FromString(ArgumentInfo.Description))
			.IsChecked(bDefaultValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([InOutValue](ECheckBoxState State) { *InOutValue = (State == ECheckBoxState::Checked ? "1" : "0"); });
		}
		case EArgumentStyle::DropdownText:
		{
			// Copy all available options into a shared array so we have a stable base for filtering
			TSharedPtr<TArray<TSharedPtr<FString>>> AllOptions = MakeShared<TArray<TSharedPtr<FString>>>();
			if (const TArray<TSharedPtr<FString>>* SourceOptions = ArgumentInfo.OptionsSource.GetOptions(FindPlayWorld()))
			{
				*AllOptions = *SourceOptions;
			}

			if (InOutValue->IsEmpty() && !AllOptions->IsEmpty())
			{
				*InOutValue = *(*AllOptions)[0];
			}

			// FilteredOptions is what the combobox reads from; we mutate it as the user types
			TSharedPtr<TArray<TSharedPtr<FString>>> FilteredOptions = MakeShared<TArray<TSharedPtr<FString>>>(*AllOptions);

			const FGuid ComboboxId = FGuid::NewGuid();
			return SAssignNew(CheatMenuComboBoxPointers.Add(ComboboxId), SComboBox<TSharedPtr<FString>>)
			.IsFocusable(true)
			.EnableGamepadNavigationMode(true)
			.CollapseMenuOnParentFocus(false)
			.ToolTipText(FText::FromString(ArgumentInfo.Description))
			.OptionsSource(FilteredOptions.Get())
			.Method(EPopupMethod::UseCurrentWindow)
			.OnComboBoxOpening_Lambda([ComboboxId]()
			{
				// Reset UI focus to the combobox button to avoid that the dropdown menu closes again after moving the mouse (engine bug):
				TWeakPtr<SComboBox<TSharedPtr<FString>>>* ComboBox = CheatMenuComboBoxPointers.Find(ComboboxId);
				if (ComboBox && ComboBox->IsValid())
				{
					ComboBox->Pin()->RefreshOptions();
					ComboBox->Pin()->SetMenuContentWidgetToFocus(ComboBox->Pin());
				}
			})
			.OnSelectionChanged_Lambda([InOutValue, AllOptions, FilteredOptions](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
			{
				if (NewSelection.IsValid())
				{
					*InOutValue = *NewSelection;
					*FilteredOptions = *AllOptions; // restore full list after a selection
				}
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
			{
				return SNew(STextBlock)
				.Text(FText::FromString(*Option))
				.Font(GetDefaultCheatMenuTextFont())
				.MinDesiredWidth(MinDesiredWith * 3.f);
			})
			[
				SNew(SEditableText)
				.Text_Lambda([InOutValue](){ return FText::FromString(*InOutValue); })
				.Font(GetDefaultCheatMenuTextFont())
				.Justification(ETextJustify::Right)
				.MinDesiredWidth(MinDesiredWith * 3.f)
				.OverflowPolicy(ETextOverflowPolicy::MiddleEllipsis)
				.OnTextChanged_Lambda([InOutValue, AllOptions, FilteredOptions, ComboboxId](const FText& NewText)
				{
					*InOutValue = NewText.ToString();
					const FString& FilterStr = *InOutValue;

					FilteredOptions->Empty();
					for (const TSharedPtr<FString>& Option : *AllOptions)
					{
						if (FilterStr.IsEmpty() || Option->Contains(FilterStr))
						{
							FilteredOptions->Add(Option);
						}
					}

					TWeakPtr<SComboBox<TSharedPtr<FString>>>* ComboBoxPtr = CheatMenuComboBoxPointers.Find(ComboboxId);
					if (ComboBoxPtr && ComboBoxPtr->IsValid())
					{
						TSharedPtr<SComboBox<TSharedPtr<FString>>> PinnedComboBox = ComboBoxPtr->Pin();
						PinnedComboBox->RefreshOptions();
						if (!PinnedComboBox->IsOpen())
						{
							PinnedComboBox->SetIsOpen(true, false);
						}
					}
				})
			];
		}
		default: case EArgumentStyle::Text:
		{
			const FString DefaultValue = *InOutValue;
			return SNew(SEditableTextBox)
			.MinDesiredWidth(MinDesiredWith * 3.f)
			.ToolTipText(FText::FromString(ArgumentInfo.Description))
			.Font(GetDefaultCheatMenuTextFont())
			.OnTextCommitted_Lambda([InOutValue](const FText& Text, ETextCommit::Type) { *InOutValue = Text.ToString(); });
		}
	}
}

void SCheatMenu::HandleFilterTextChanged(const FText& NewFilterText)
{
	TextFilteredCheatMenuActions.Empty();
	if (NewFilterText.IsEmpty())
	{
		FilterText.Reset();
		CurrentTabName = DEFAULT_TAB_NAME;
	}
	else
	{
		FilterText = NewFilterText;
		CurrentTabName = FILTER_RESULT_TAB_NAME;

		Algo::TransformIf(Entries, OUT TextFilteredCheatMenuActions,
			/*Condition*/ [NewFilterText](const TSharedPtr<FEntry>& Entry){ return Entry->MatchesFilterText(NewFilterText.ToString()); },
			/*Transform*/ [](const TSharedPtr<FEntry>& Entry){ return Entry->GetCommandName(); });
	}

	RefreshTabContent();
}

void SCheatMenu::HandleCheatLogMessage(const ICheatMenuAction& Command, ELogVerbosity::Type Verbosity, const FString& Message)
{
	EStyleColor Color;
	FText PrefixIcon = FText();
	switch (Verbosity)
	{
		case ELogVerbosity::Fatal:
		case ELogVerbosity::Error:
			Color = EStyleColor::AccentRed;
			PrefixIcon = INVTEXT("⛔ ");
			break;

		case ELogVerbosity::Warning:
			Color = EStyleColor::AccentYellow;
			PrefixIcon = INVTEXT("⚠ ");
			break;

		default: // (i) Do not show normie logs in the error/warning box.
			ErrorText->SetText(FText());
			return;
	}

	ErrorText->SetText(FText::Format(INVTEXT("{0}[{1}] {2}"), PrefixIcon, FText::FromString(Command.GetName()), FText::FromString(Message)));
	ErrorText->SetColorAndOpacity(Color);
}

void SCheatMenu::HandleCheatExecuted(const ICheatMenuAction& CheatMenuAction, UWorld* World, TArray<FString> Args)
{
	InsertCheatIntoRecentlyUsedList(CheatMenuAction.GetName());
	OnCheatExecuted.ExecuteIfBound(CheatMenuAction, World, Args);
}

void SCheatMenu::InsertCheatIntoRecentlyUsedList(const FString& CheatName)
{
	RecentlyUsedCheatMenuActions.Remove(CheatName);
	RecentlyUsedCheatMenuActions.Insert(CheatName, 0);

	if (RecentlyUsedCheatMenuActions.Num() > NumRecentlyUsedCheatsToShow)
	{
		RecentlyUsedCheatMenuActions.SetNum(NumRecentlyUsedCheatsToShow);
	}

	SaveFavoriteAndRecentlyUsedCheats();

	if (CurrentTabName == RECENTLY_USED_TAB_NAME)
	{
		RefreshTabContent();
	}
}

FReply SCheatMenu::HandleCheatFavoriteButtonClicked(FString CheatName)
{
	ToggleCheatForFavorites(CheatName);
	return FReply::Handled();
}

void SCheatMenu::ToggleCheatForFavorites(const FString& CheatName)
{
	if (FavoriteCheatMenuActions.Contains(CheatName))
	{
		FavoriteCheatMenuActions.Remove(CheatName);
	}
	else
	{
		FavoriteCheatMenuActions.Add(CheatName);
	}

	SaveFavoriteAndRecentlyUsedCheats();

	if (CurrentTabName == FAVORITE_TAB_NAME)
	{
		RefreshTabContent();
	}
}

void SCheatMenu::SaveFavoriteAndRecentlyUsedCheats() const
{
	if (GConfig != nullptr)
	{
		GConfig->SetArray(CHEAT_MENU_INI_SECTION, CHEAT_MENU_INI_FAVORITES, FavoriteCheatMenuActions, CHEAT_MENU_INI_FILE);
		GConfig->SetArray(CHEAT_MENU_INI_SECTION, CHEAT_MENU_INI_RECENTLY_USED, RecentlyUsedCheatMenuActions, CHEAT_MENU_INI_FILE);
		GConfig->Flush(false, CHEAT_MENU_INI_FILE);
	}
}

void SCheatMenu::RestoreFavoriteAndRecentlyUsedCheats()
{
	if (GConfig != nullptr)
	{
		GConfig->GetArray(CHEAT_MENU_INI_SECTION, CHEAT_MENU_INI_FAVORITES, OUT FavoriteCheatMenuActions, CHEAT_MENU_INI_FILE);
		GConfig->GetArray(CHEAT_MENU_INI_SECTION, CHEAT_MENU_INI_RECENTLY_USED, OUT RecentlyUsedCheatMenuActions, CHEAT_MENU_INI_FILE);
	}
}
