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

#if WITH_EDITOR

namespace WeekendUtils
{
	/**
	 * Opens the Session Frontend's Automation panel and enables (checks) the given tests once they have
	 * been discovered.
	 * @param TestNames Full test names to enable.
	 */
	WEEKENDUTILS_API void OpenTestsInAutomationTestFrontend(const TArray<FString>& TestNames);
}

#endif
