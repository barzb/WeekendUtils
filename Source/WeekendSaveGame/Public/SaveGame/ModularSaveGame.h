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
#include "GameFramework/SaveGame.h"
#include "SaveGame/SaveGameHeader.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/SaveGameSerializer.h"
#include "StructUtils/InstancedStruct.h"

#include "ModularSaveGame.generated.h"

/**
 * SaveGame implementation that uses polymorphic subobjects called "SaveGameModules".
 * Additionally, this class supports setting a custom HeaderData struct that is supposed
 * to contain meta information about a SaveGame object.
 */
UCLASS(EditInlineNew)
class WEEKENDSAVEGAME_API UModularSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UModularSaveGame()
	{
		CreateHeaderData(FSimpleSaveGameHeaderData());
	}

	///////////////////////////////////////////////////////////////////////////////////////
	/// MODULES

	/** @returns the CurrentSaveGame of the @USaveGameService as ModularSaveGame - or nullptr. */
	static const UModularSaveGame* FindCurrent(const UObject* WorldContext);

	/** @returns the CurrentSaveGame of the @USaveGameService as ModularSaveGame - or nullptr. */
	static UModularSaveGame* FindMutableCurrent(const UObject* WorldContext);

	/**
	 * @returns requested module instance for the summoner, either from the current ModularSaveGame
	 * or - as fallback - a new instance exclusively for the summoner, not bound to the CurrentSaveGame.
	 * References to summoned modules should be stored as TObjectPtr<> by the summoner for lifetime safety.
	 * */
	template <typename T>
	static T& SummonModule(UObject& Summoner, const TSubclassOf<T>& ModuleClass = T::StaticClass());
	template <typename T>
	static T& SummonModule(UObject& Summoner, const FName& ModuleName, const TSubclassOf<T>& ModuleClass = T::StaticClass());

	template <typename T>
	T& FindOrAddModule(const TSubclassOf<T>& ModuleClass = T::StaticClass());
	template <typename T>
	T& FindOrAddModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass = T::StaticClass());

	template <typename T>
	T* FindModule(const TSubclassOf<T>& ModuleClass = T::StaticClass());
	template <typename T>
	T* FindModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass = T::StaticClass());

	template <typename T>
	const T* FindModule(const TSubclassOf<T>& ModuleClass = T::StaticClass()) const;
	template <typename T>
	const T* FindModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass = T::StaticClass()) const;

	template <typename T>
	bool HasModule(const TSubclassOf<T>& ModuleClass = T::StaticClass()) const;
	template <typename T>
	bool HasModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass = T::StaticClass()) const;
	bool HasModule(const FName& ModuleName) const { return Modules.Contains(ModuleName); }

	template <typename T>
	bool DeleteModule(const TSubclassOf<T>& ModuleClass = T::StaticClass());
	bool DeleteModule(const FName& ModuleName) { return (Modules.Remove(ModuleName) > 0); }

	void ForEachModule(const TFunction<void(const FName&, USaveGameModule&)>& Function);
	void ForEachModule(const TFunction<void(const FName&, const USaveGameModule&)>& Function) const;

	///////////////////////////////////////////////////////////////////////////////////////
	/// HEADER

	template <typename T>
	TSharedPtr<FInstancedStruct> CreateHeaderData(const T& HeaderData);

	template <typename T>
	const T& GetHeaderData() const;

	template <typename T>
	T& GetMutableHeaderData() const;

	template <typename T>
	const T* GetHeaderDataPtr() const;

	template <typename T>
	T* GetMutableHeaderDataPtr() const;

	TSharedPtr<FInstancedStruct> GetInstancedHeaderData() const { return InstancedHeaderData; }
	void SetInstancedHeaderData(const FInstancedStruct& HeaderData) { InstancedHeaderData = MakeShared<FInstancedStruct>(HeaderData); }

	///////////////////////////////////////////////////////////////////////////////////////
	/// UTILS

#if WITH_EDITOR
	/** Analyzes the composition of this ModularSaveGame and reports it to the MessageLog. */
	virtual void AnalyzeAndReportSaveGameComposition() const;
#endif

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(SaveGame, Instanced, EditDefaultsOnly, Category = "Modular Save Game")
	TMap<FName, TObjectPtr<USaveGameModule>> Modules = {};

	TSharedPtr<FInstancedStruct> InstancedHeaderData = nullptr;
};

///////////////////////////////////////////////////////////////////////////////////////
/// SERIALIZER

/**
 * Custom serializer for the @UModularSaveGame, to be used within the @USaveGameService.
 */
UCLASS()
class WEEKENDSAVEGAME_API UModularSaveGameSerializer : public USaveGameSerializer
{
	GENERATED_BODY()

public:
	// - USaveGameSerializer
	virtual bool TrySerializeSaveGame(USaveGame& InSaveGameObject, TArray<uint8>& OutSaveData) const override;
	virtual bool TryDeserializeSaveGame(const TArray<uint8>& InSaveData, USaveGame*& OutSaveGameObject) const override;
	// --
};

///////////////////////////////////////////////////////////////////////////////////////
/// TEMPLATES @UModularSaveGame

template <typename T>
T& UModularSaveGame::SummonModule(UObject& Summoner, const TSubclassOf<T>& ModuleClass)
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	UModularSaveGame* SaveGame = FindMutableCurrent(&Summoner);
	return (IsValid(SaveGame) ? SaveGame->FindOrAddModule<T>(ModuleClass) : *NewObject<T>(&Summoner, ModuleClass));
}

template <typename T>
T& UModularSaveGame::SummonModule(UObject& Summoner, const FName& ModuleName, const TSubclassOf<T>& ModuleClass)
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	UModularSaveGame* SaveGame = FindMutableCurrent(&Summoner);
	return (IsValid(SaveGame) ? SaveGame->FindOrAddModule<T>(ModuleName, ModuleClass) : *NewObject<T>(&Summoner, ModuleClass));
}

template <typename T>
T& UModularSaveGame::FindOrAddModule(const TSubclassOf<T>& ModuleClass)
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	const FName& DefaultModuleName = GetDefault<T>(ModuleClass)->DefaultModuleName;
	return FindOrAddModule<T>(DefaultModuleName, ModuleClass);
}

template <typename T>
T& UModularSaveGame::FindOrAddModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass)
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	if (T* ExistingModule = FindModule<T>(ModuleName, ModuleClass))
	{
		ensure(ExistingModule->GetClass() == ModuleClass);
		return *ExistingModule;
	}

	T* NewModule = NewObject<T>(this, ModuleClass);
	Modules.Add(ModuleName, NewModule);
	checkf(ModuleName != NAME_None, TEXT("Tried to add module %s with unset name. ModuleName must not be empty or unset!."),
		*ModuleClass->GetName(), *ModuleName.ToString());
	return *NewModule;
}

template <typename T>
T* UModularSaveGame::FindModule(const TSubclassOf<T>& ModuleClass)
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	const FName& DefaultModuleName = GetDefault<T>(ModuleClass)->DefaultModuleName;
	return FindModule<T>(DefaultModuleName, ModuleClass);
}

template <typename T>
T* UModularSaveGame::FindModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass)
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	const FName& FindModuleName = (ModuleName.IsNone() ? GetDefault<T>(ModuleClass)->DefaultModuleName : ModuleName);
	auto* FoundModule = Modules.Find(FindModuleName);
	if (FoundModule)
	{
		ensureAlwaysMsgf(FoundModule->GetClass() == ModuleClass, 
			TEXT("Tried finding Module of Class %s with Name %s, but instead found module of class %s with name %s."), 
			*ModuleClass.Get()->GetName(), *ModuleName.ToString(), *FoundModule->GetClass()->GetName(), *FindModuleName.ToString());
	}
	return ((FoundModule && FoundModule->GetClass() == ModuleClass) ? Cast<T>(FoundModule->Get()) : nullptr);
}

template <typename T>
const T* UModularSaveGame::FindModule(const TSubclassOf<T>& ModuleClass) const
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	const FName& DefaultModuleName = GetDefault<T>(ModuleClass)->DefaultModuleName;
	return FindModule<T>(DefaultModuleName, ModuleClass);
}

template <typename T>
const T* UModularSaveGame::FindModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass) const
{
	static_assert(TIsDerivedFrom<T, USaveGameModule>::IsDerived, "Type is not derived from USaveGameModule.");
	const FName& FindModuleName = (ModuleName.IsNone() ? GetDefault<T>(ModuleClass)->DefaultModuleName : ModuleName);
	const auto* FoundModule = Modules.Find(FindModuleName);
	return ((FoundModule && FoundModule->GetClass() == ModuleClass) ? Cast<T>(FoundModule->Get()) : nullptr);
}

template <typename T>
bool UModularSaveGame::HasModule(const TSubclassOf<T>& ModuleClass) const
{
	return HasModule(GetDefault<T>(ModuleClass)->DefaultModuleName, ModuleClass);
}

template <typename T>
bool UModularSaveGame::HasModule(const FName& ModuleName, const TSubclassOf<T>& ModuleClass) const
{
	static_assert(!TIsAbstract<T>::Value, "Type is abstract.");
	const auto* FoundModule = FindModule<T>(ModuleName, ModuleClass);
	return (FoundModule && (FoundModule->GetClass() == ModuleClass));
}

template <typename T>
bool UModularSaveGame::DeleteModule(const TSubclassOf<T>& ModuleClass)
{
	return DeleteModule(GetDefault<T>(ModuleClass)->DefaultModuleName);
}

template <typename T>
TSharedPtr<FInstancedStruct> UModularSaveGame::CreateHeaderData(const T& HeaderData)
{
	static_assert(TIsDerivedFrom<T, FSaveGameHeaderDataBase>::IsDerived, "Type is not derived from FSaveGameHeaderDataBase");
	InstancedHeaderData = MakeShared<FInstancedStruct>(FInstancedStruct::Make<T>(HeaderData));
	return InstancedHeaderData;
}

template <typename T>
const T& UModularSaveGame::GetHeaderData() const
{
	return InstancedHeaderData->Get<T>();
}

template <typename T>
T& UModularSaveGame::GetMutableHeaderData() const
{
	return InstancedHeaderData->GetMutable<T>();
}

template <typename T>
const T* UModularSaveGame::GetHeaderDataPtr() const
{
	if (!InstancedHeaderData.IsValid())
		return nullptr;

	return InstancedHeaderData->GetPtr<T>();
}

template <typename T>
T* UModularSaveGame::GetMutableHeaderDataPtr() const
{
	if (!InstancedHeaderData.IsValid())
		return nullptr;

	return InstancedHeaderData->GetMutablePtr<T>();
}
