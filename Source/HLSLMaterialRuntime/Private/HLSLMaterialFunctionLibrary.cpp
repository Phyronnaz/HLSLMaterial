// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionLibrary.h"

#if WITH_EDITOR
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

UHLSLMaterialFunctionLibrary::FOnUpdate UHLSLMaterialFunctionLibrary::OnUpdate;

FString UHLSLMaterialFunctionLibrary::GetFilePath() const
{
	return GetFilePath(File.FilePath);
}

FString UHLSLMaterialFunctionLibrary::GetFilePath(const FString& InFilePath)
{
	FString FullPath;
	
	if (FPackageName::TryConvertLongPackageNameToFilename(InFilePath, FullPath))
	{
		return FullPath;
	}
	
	FullPath = FPaths::ConvertRelativePathToFull(InFilePath);
	
	return FullPath;
}

void UHLSLMaterialFunctionLibrary::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (bUpdateOnFileChange && !WatcherDelegate.IsValid())
	{
		BindWatcher();
	}
	if (!bUpdateOnFileChange && WatcherDelegate.IsValid())
	{
		UnbindWatcher();
	}

	if (!PropertyChangedEvent.MemberProperty || 
		PropertyChangedEvent.MemberProperty->GetFName() != GET_MEMBER_NAME_CHECKED(UHLSLMaterialFunctionLibrary, File))
	{
		return;
	}

	if (bUpdateOnFileChange)
	{
		UnbindWatcher();
		BindWatcher();
	}

	FString NewPath = File.FilePath;
	MakeRelativePath(NewPath);

	if (FPaths::ConvertRelativePathToFull(GetFilePath(File.FilePath)) ==
		FPaths::ConvertRelativePathToFull(GetFilePath(NewPath)))
	{
		// Conversion is safe
		File.FilePath = NewPath;
	}
}

void UHLSLMaterialFunctionLibrary::BeginDestroy()
{
	Super::BeginDestroy();

	UnbindWatcher();
}

void UHLSLMaterialFunctionLibrary::PostLoad()
{
	Super::PostLoad();

	if (bUpdateOnFileChange && !WatcherDelegate.IsValid())
	{
		BindWatcher();
	}
}

void UHLSLMaterialFunctionLibrary::BindWatcher()
{
	ensure(WatchedDirectory.IsEmpty());
	ensure(!WatcherDelegate.IsValid());

	WatchedDirectory = FPaths::GetPath(GetFilePath());

	if (WatchedDirectory.IsEmpty() || 
		!FPaths::DirectoryExists(WatchedDirectory))
	{
		WatchedDirectory.Reset();
		return;
	}

	FDirectoryWatcherModule& Module = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (IDirectoryWatcher* DirectoryWatcher = Module.Get())
	{
		const IDirectoryWatcher::FDirectoryChanged Callback = IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UHLSLMaterialFunctionLibrary::OnDirectoryChanged);
		ensure(DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(WatchedDirectory, Callback, WatcherDelegate));
	}
}

void UHLSLMaterialFunctionLibrary::UnbindWatcher()
{
	ensure(WatchedDirectory.IsEmpty() == !WatcherDelegate.IsValid());

	if (!WatcherDelegate.IsValid())
	{
		return;
	}
	ensure(!WatchedDirectory.IsEmpty());

	if (FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* DirectoryWatcher = Module->Get())
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(WatchedDirectory, WatcherDelegate);
		}
	}

	WatchedDirectory.Reset();
	WatcherDelegate.Reset();
}

void UHLSLMaterialFunctionLibrary::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	const FString FilePath = GetFilePath();
	for (const FFileChangeData& FileChange : FileChanges)
	{
		if (FileChange.Filename == FilePath)
		{
			Update();
			break;
		}
	}
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
	return;
}
#endif