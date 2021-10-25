// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionLibrary.h"

#if WITH_EDITOR
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

UHLSLMaterialFunctionLibrary::FOnUpdate UHLSLMaterialFunctionLibrary::OnUpdate;

FString UHLSLMaterialFunctionLibrary::GetFilePath() const
{
	FString FullPath = File.FilePath;
	if (!FPaths::FileExists(FullPath))
	{
		FullPath = FPaths::ProjectDir() / FullPath;
	}
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

	const FString AbsoluteProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString AbsolutePickedPath = FPaths::ConvertRelativePathToFull(File.FilePath);

	if (!FPaths::FileExists(AbsolutePickedPath))
	{
		// If absolute file doesn't exist, it might already be relative to project dir
		// If not, then it might be a manual entry, so keep it untouched either way
		return;
	}

	if (!AbsolutePickedPath.StartsWith(AbsoluteProjectDir))
	{
		return;
	}

	File.FilePath = AbsolutePickedPath.RightChop(AbsoluteProjectDir.Len());

	if (bUpdateOnFileChange)
	{
		UnbindWatcher();
		BindWatcher();
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

	if (WatchedDirectory.IsEmpty())
	{
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
#endif