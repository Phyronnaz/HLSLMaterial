// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionLibraryEditor.h"
#include "HLSLMaterialFunction.h"
#include "HLSLMaterialFunctionLibrary.h"
#include "HLSLMaterialFunctionGenerator.h"
#include "HLSLMaterialParser.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialFileWatcher.h"
#include "HLSLMaterialMessages.h"

#include "Misc/FileHelper.h"
#include "AssetRegistry/AssetRegistryModule.h"

class FHLSLMaterialEditorInterfaceImpl : public IHLSLMaterialEditorInterface
{
public:
	virtual TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLMaterialFunctionLibrary& Library) override
	{
		return FHLSLMaterialFunctionLibraryEditor::CreateWatcher(Library);
	}
	virtual void Update(UHLSLMaterialFunctionLibrary& Library) override
	{
		FHLSLMaterialFunctionLibraryEditor::Generate(Library);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FHLSLMaterialFunctionLibraryEditor::Register()
{
	IHLSLMaterialEditorInterface::StaticInterface = new FHLSLMaterialEditorInterfaceImpl();

	// Delay to ensure the editor is fully loaded before searching for assets
	FHLSLMaterialUtilities::DelayedCall([]
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Force load all libraries that have bGenerateOnFileChange, to start their watchers
		TArray<FAssetData> AssetDatas;
		FARFilter Filer;
		Filer.ClassNames.Add(UHLSLMaterialFunctionLibrary::StaticClass()->GetFName());
		Filer.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UHLSLMaterialFunctionLibrary, bUpdateOnFileChange), FString("true"));
		AssetRegistryModule.Get().GetAssets(Filer, AssetDatas);

		for (const FAssetData& AssetData : AssetDatas)
		{
			ensure(AssetData.GetAsset());
		}
	});
}
HLSL_STARTUP_FUNCTION(EDelayedRegisterRunPhase::EndOfEngineInit, FHLSLMaterialFunctionLibraryEditor::Register);

TSharedRef<FVirtualDestructor> FHLSLMaterialFunctionLibraryEditor::CreateWatcher(UHLSLMaterialFunctionLibrary& Library)
{
	FHLSLMaterialMessages::FLibraryScope Scope(Library);

	TArray<FString> Files;
	Files.Add(Library.GetFilePath());

	FString Text;
	if (TryLoadFileToString(Text, Library.GetFilePath()))
	{
		for (const FHLSLMaterialParser::FInclude& Include : FHLSLMaterialParser::GetIncludes(Text))
		{
			if (!Include.DiskPath.IsEmpty())
			{
				Files.Add(Include.DiskPath);
			}
		}
	}

	const TSharedRef<FHLSLMaterialFileWatcher> Watcher = FHLSLMaterialFileWatcher::Create(Files);
	Watcher->OnFileChanged.AddWeakLambda(&Library, [&Library]
	{
		Generate(Library);
	});

	return Watcher;
}

void FHLSLMaterialFunctionLibraryEditor::Generate(UHLSLMaterialFunctionLibrary& Library)
{
	FHLSLMaterialMessages::FLibraryScope Scope(Library);

	// Always recreate watcher in case includes changed
	Library.CreateWatcherIfNeeded();

	const FString FullPath = Library.GetFilePath();
	
	FString Text;
	if (!TryLoadFileToString(Text, FullPath))
	{
		FHLSLMaterialMessages::ShowError(TEXT("Failed to read %s"), *FullPath);
		return;
	}
	
	FString BaseHash;
	TArray<FString> IncludeFilePaths;
	for (const FHLSLMaterialParser::FInclude& Include : FHLSLMaterialParser::GetIncludes(Text))
	{
		IncludeFilePaths.Add(Include.VirtualPath);

		FString IncludeText;
		if (TryLoadFileToString(IncludeText, Include.DiskPath))
		{
			BaseHash += FHLSLMaterialUtilities::HashString(IncludeText);
		}
		else
		{
			FHLSLMaterialMessages::ShowError(TEXT("Invalid include: %s"), *Include.VirtualPath);
		}
	}

	const TArray<FCustomDefine> AdditionalDefines = FHLSLMaterialParser::GetDefines(Text);
	for (const FCustomDefine& Define : AdditionalDefines)
	{
		BaseHash += FHLSLMaterialUtilities::HashString(Define.DefineName);
		BaseHash += FHLSLMaterialUtilities::HashString(Define.DefineValue);
	}

	TArray<FHLSLMaterialFunction> Functions;
	{
		const FString Error = FHLSLMaterialParser::Parse(Library, Text, Functions);
		if (!Error.IsEmpty())
		{
			FHLSLMaterialMessages::ShowError(TEXT("Parsing failed: %s"), *Error);
			return;
		}
	}

	Library.MaterialFunctions.RemoveAll([&](TSoftObjectPtr<UMaterialFunction> InFunction)
	{
		return !InFunction.LoadSynchronous();
	});
	
	FMaterialUpdateContext UpdateContext;
	for (FHLSLMaterialFunction Function : Functions)
	{
		Function.HashedString = Function.GenerateHashedString(BaseHash);
		
		const FString Error = FHLSLMaterialFunctionGenerator::GenerateFunction(
			Library, 
			IncludeFilePaths, 
			AdditionalDefines, 
			Function, 
			UpdateContext);

		if (!Error.IsEmpty())
		{
			FHLSLMaterialMessages::ShowError(TEXT("Function %s: %s"), *Function.Name, *Error);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FHLSLMaterialFunctionLibraryEditor::TryLoadFileToString(FString& Text, const FString& FullPath)
{
	if (!FPaths::FileExists(FullPath))
	{
		return false;
	}

	if (!FFileHelper::LoadFileToString(Text, *FullPath))
	{
		// Wait and retry in case the text editor has locked the file
		FPlatformProcess::Sleep(0.1f);

		if (!FFileHelper::LoadFileToString(Text, *FullPath))
		{
			UE_LOG(LogHLSLMaterial, Error, TEXT("Failed to read %s"), *FullPath);
			return false;
		}
	}

	return true;
}