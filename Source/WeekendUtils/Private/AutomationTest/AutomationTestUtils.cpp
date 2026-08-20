///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "AutomationTest/AutomationTestUtils.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "IAutomationControllerModule.h"
#include "ISessionFrontendModule.h"
#include "Modules/ModuleManager.h"

namespace
{
	/**
	 * Keeps a desired test selection applied across the asynchronous discovery passes that the Automation
	 * frontend performs after it opens. Re-applies the selection on every OnTestsRefreshed broadcast and only
	 * finalizes (applies a last time, then stops listening) once the refreshes have gone quiet for a short
	 * debounce window - so the last report rebuild during discovery is always the one we win. Self-owning:
	 * the active instance is held alive by the static handle below and released when it finalizes or is replaced.
	 */
	class FAutomationTestSelectionApplier : public TSharedFromThis<FAutomationTestSelectionApplier>
	{
	public:
		FAutomationTestSelectionApplier(const IAutomationControllerManagerRef& InController, const TArray<FString>& InTestNames)
			: Controller(InController), TestNames(InTestNames)
		{
		}

		~FAutomationTestSelectionApplier()
		{
			Cancel();
		}

		static void Start(const TArray<FString>& InTestNames)
		{
			// Replace any in-flight selection from a previous call before starting a new one.
			if (ActiveApplier.IsValid())
			{
				ActiveApplier->Cancel();
			}

			IAutomationControllerModule& AutomationModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>(TEXT("AutomationController"));
			const IAutomationControllerManagerRef Controller = AutomationModule.GetAutomationController();
			ActiveApplier = MakeShared<FAutomationTestSelectionApplier>(Controller, InTestNames);
			ActiveApplier->Begin();
		}

	private:
		void Begin()
		{
			TWeakPtr<FAutomationTestSelectionApplier> WeakSelf = AsShared();
			RefreshHandle = Controller->OnTestsRefreshed().AddLambda([WeakSelf]()
			{
				if (const TSharedPtr<FAutomationTestSelectionApplier> Self = WeakSelf.Pin())
				{
					Self->Apply();
				}
			});

			// Warm case: reports may already be present, so apply immediately too.
			Apply();
		}

		void Apply()
		{
			Controller->SetEnabledTests(TestNames);
			ArmFinalizeTimer();
		}

		void ArmFinalizeTimer()
		{
			if (FinalizeHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(FinalizeHandle);
				FinalizeHandle.Reset();
			}

			TWeakPtr<FAutomationTestSelectionApplier> WeakSelf = AsShared();
			FinalizeHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[WeakSelf](float /*DeltaTime*/) -> bool
				{
					if (const TSharedPtr<FAutomationTestSelectionApplier> Self = WeakSelf.Pin())
					{
						Self->Finalize();
					}
					return false; // One-shot: returning false unregisters this ticker.
				}), SettleDelaySeconds);
		}

		void Finalize()
		{
			// Discovery has been quiet for the debounce window: apply a final time, then release ourselves.
			Controller->SetEnabledTests(TestNames);
			FinalizeHandle.Reset(); // The firing ticker self-unregisters by returning false.
			Cancel();
		}

		void Cancel()
		{
			if (RefreshHandle.IsValid())
			{
				Controller->OnTestsRefreshed().Remove(RefreshHandle);
				RefreshHandle.Reset();
			}

			if (FinalizeHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(FinalizeHandle);
				FinalizeHandle.Reset();
			}

			// Drop the static reference last; a TSharedPtr captured by the firing ticker keeps us alive
			// for the remainder of the current call.
			if (ActiveApplier.Get() == this)
			{
				ActiveApplier.Reset();
			}
		}

		static constexpr float SettleDelaySeconds = 2.0f;
		static TSharedPtr<FAutomationTestSelectionApplier> ActiveApplier;

		IAutomationControllerManagerRef Controller;
		TArray<FString> TestNames;
		FDelegateHandle RefreshHandle;
		FTSTicker::FDelegateHandle FinalizeHandle;
	};

	TSharedPtr<FAutomationTestSelectionApplier> FAutomationTestSelectionApplier::ActiveApplier = nullptr;
}

void WeekendUtils::OpenTestsInAutomationTestFrontend(const TArray<FString>& TestNames)
{
	ISessionFrontendModule& SessionFrontendModule = FModuleManager::LoadModuleChecked<ISessionFrontendModule>(TEXT("SessionFrontend"));
	SessionFrontendModule.InvokeSessionFrontend(FName("AutomationPanel"));

	if (TestNames.Num() > 0)
	{
		FAutomationTestSelectionApplier::Start(TestNames);
	}
}

#endif
