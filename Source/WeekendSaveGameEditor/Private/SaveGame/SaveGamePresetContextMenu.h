///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"

namespace WeekendUtils
{
	/** Register context menu actions for USaveGamePreset assets with the editor. */
	void RegisterSaveGamePresetContextMenu();
	/** Unregister context menu actions for USaveGamePreset assets from the editor. */
	void UnregisterSaveGamePresetContextMenu();
}
