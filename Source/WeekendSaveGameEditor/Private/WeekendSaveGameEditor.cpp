///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#include "WeekendSaveGameEditor.h"

#include "SaveGame/SaveGamePresetContextMenu.h"
#include "ToolMenus.h"

IMPLEMENT_MODULE(FWeekendSaveGameEditorModule, WeekendSaveGameEditor)

void FWeekendSaveGameEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&WeekendUtils::RegisterSaveGamePresetContextMenu));
}

void FWeekendSaveGameEditorModule::ShutdownModule()
{
	WeekendUtils::UnregisterSaveGamePresetContextMenu();
}
