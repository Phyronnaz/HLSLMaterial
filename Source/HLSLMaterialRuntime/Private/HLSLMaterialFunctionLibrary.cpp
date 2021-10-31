// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionLibrary.h"

#if WITH_EDITOR
IHLSLMaterialEditorInterface* IHLSLMaterialEditorInterface::StaticInterface = nullptr;

FString UHLSLMaterialFunctionLibrary::GetFilePath() const
{
	return GetFilePath(File.FilePath);
}

FString UHLSLMaterialFunctionLibrary::GetFilePath(const FString& InFilePath)
{
	FString FullPath;

	// Try to convert from /Game/Smthg to the full filename
	if (!FPackageName::TryConvertLongPackageNameToFilename(InFilePath, FullPath))
	{
		FullPath = InFilePath;
	}

	// Always return the full path
	return FPaths::ConvertRelativePathToFull(FullPath);
}

void UHLSLMaterialFunctionLibrary::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// This is overkill, but better be safe with keeping things up to date

	FString NewPath = File.FilePath;
	MakeRelativePath(NewPath);

	if (GetFilePath(File.FilePath) == GetFilePath(NewPath))
	{
		// Conversion is safe
		File.FilePath = NewPath;
	}

	Watcher.Reset();
	if (bUpdateOnFileChange)
	{
		CreateWatcher();
	}
}

void UHLSLMaterialFunctionLibrary::BeginDestroy()
{
	Super::BeginDestroy();
	
	Watcher.Reset();
}

void UHLSLMaterialFunctionLibrary::PostLoad()
{
	Super::PostLoad();

	if (bUpdateOnFileChange)
	{
		CreateWatcher();
	}
}

void UHLSLMaterialFunctionLibrary::CreateWatcher()
{
	TArray<FString> Files;
	Files.Add(GetFilePath());

	for (const FString& IncludeFilePath : IncludeFilePaths)
	{
		if (IncludeFilePath.IsEmpty())
		{
			continue;
		}

		const FString MappedInclude = GetShaderSourceFilePath(IncludeFilePath);
		if (MappedInclude.IsEmpty())
		{
			continue;
		}

		Files.Add(FPaths::ConvertRelativePathToFull(MappedInclude));
	}

	Watcher = IHLSLMaterialEditorInterface::Get().CreateWatcher(*this, Files);
}

void UHLSLMaterialFunctionLibrary::MakeRelativePath(FString& Path)
{
	const FString AbsolutePickedPath = FPaths::ConvertRelativePathToFull(Path);

	if (!FPaths::FileExists(AbsolutePickedPath))
	{
		// This is either a manual entry or already a relative entry, don't do anything to it
		return;
	}

	FString PackageName;
	if (!FPackageName::TryConvertFilenameToLongPackageName(AbsolutePickedPath, PackageName))
	{
		return;
	}

	Path = PackageName + "." + FPaths::GetExtension(Path);
}
#endif