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

#include "Misc/AutomationTest.h"

//////////////////////////////////////////////////////////////////////////
// This header file contains utility macros that should aid in the creation
// of automated tests. They work in conjunction with the macros defined in
// AutomationTest.h and are not supposed to be a complete replacement,
// but rather an augmentation of the latter.
//
// All of the macros in this file must contain the initials WE to avoid
// name clashes and make them easily distinguishable!
//////////////////////////////////////////////////////////////////////////

	/**
	 * Use this macro in conjunction with a predefined SPEC_TEST_CATEGORY to declare the beginning and ending of an
	 * automation spec. Used like this:
	 * -------------------------
	 * #define SPEC_TEST_CATEGORY WeekendUtils.Foo
	 * WE_BEGIN_DEFINE_SPEC(HelloWorld)
	 *     // spec class members
	 * WE_END_DEFINE_SPEC(HelloWorld)
	 * {
	 *     // spec implementation
	 * }
	 * #undef SPEC_TEST_CATEGORY
	 * -------------------------
	 * ...which declares a spec class FHelloWorldSpec in the "WeekendUtils.Foo" category.
	 */
	#define WE_BEGIN_DEFINE_SPEC(SpecName) \
		BEGIN_DEFINE_SPEC(F##SpecName##Spec, SPEC_TEST_CATEGORY "." #SpecName, EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter) \
		static inline const FString SpecTestWorldName = "FTestWorld_" #SpecName;
	#define WE_END_DEFINE_SPEC(SpecName) \
		END_DEFINE_SPEC(F##SpecName##Spec) \
		void F##SpecName##Spec::Define()
#endif
