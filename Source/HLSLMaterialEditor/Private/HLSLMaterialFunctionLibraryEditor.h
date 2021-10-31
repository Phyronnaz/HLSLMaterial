// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "HLSLMaterialFunctionLibraryEditor.generated.h"

class IMaterialEditor;
class FMessageLogListingViewModel;
class UHLSLMaterialFunctionLibrary;

UCLASS()
class UHLSLMaterialFunctionLibraryFactory : public UFactory
{
	GENERATED_BODY()

public:
	UHLSLMaterialFunctionLibraryFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface
};

class FVoxelMaterialFunctionLibraryEditor
{
public:
	static void Register();

	static TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLMaterialFunctionLibrary& Library);
	static void Generate(UHLSLMaterialFunctionLibrary& Library);

private:
	static constexpr const TCHAR* UniqueMessagePrefix = TEXT("[HLSLMaterial]");
	static constexpr const TCHAR* UniqueMessageSuffix = TEXT("[/HLSLMaterial]");

	static FString HashString(const FString& String);

	struct FFunction
	{
		int32 StartLine = 0;
		FString Comment;
		FString ReturnType;
		FString Name;
		TArray<FString> Arguments;
		FString Body;

		FString HashedString;
		
		FString GenerateHashedString(const FString& IncludesHash) const;
	};
	static FString GenerateFunction(
		UHLSLMaterialFunctionLibrary& Library,
		const TArray<FString>& IncludeFilePaths,
		FFunction Function,
		FMaterialUpdateContext& UpdateContext);

	static FString GenerateFunctionCode(const UHLSLMaterialFunctionLibrary& Library, const FFunction& Function);

private:
	static void HookMessageLogHack(IMaterialEditor& MaterialEditor);
	static void ReplaceMessages(FMessageLogListingViewModel& ViewModel);
	
private:
	static IMaterialEditor* FindMaterialEditorForAsset(UObject* InAsset);
	static UObject* CreateAsset(FString AssetName, FString FolderPath, UClass* Class, FString Suffix = {});

	template<typename T>
	static T* CreateAsset(FString AssetName, FString FolderPath, UClass* Class = nullptr, FString Suffix = {})
	{
		if (!Class)
		{
			Class = T::StaticClass();
		}
		return CastChecked<T>(CreateAsset(AssetName, FolderPath, Class, Suffix), ECastCheckedType::NullAllowed);
	}

	enum class ESeverity
	{
		Info,
		Error
	};
	static void ShowMessageImpl(ESeverity Severity, FString Message);

	template <typename FmtType, typename... Types>
	static void ShowMessage(ESeverity Severity, const FmtType& Fmt, Types... Args)
	{
		ShowMessageImpl(Severity, FString::Printf(Fmt, Args...));
	}

	static bool TryLoadFileToString(FString& Text, const FString& FullPath, const FString& LibraryName);

	static void GetIncludes(
		const FString& Text,
		const FString& LibraryName,
		TArray<FString>& OutVirtualIncludes,
		TArray<FString>& OutDiskIncludes);
};