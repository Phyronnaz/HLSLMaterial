// Copyright 2021 Phyronnaz

#include "HLSLMaterialFileWatcher.h"
#include "HLSLMaterialUtilities.h"
#include "DirectoryWatcherModule.h"

TSharedRef<FHLSLMaterialFileWatcher> FHLSLMaterialFileWatcher::Create(const TArray<FString>& InFilesToWatch)
{
	const TSharedRef<FHLSLMaterialFileWatcher> Watcher = MakeShareable(new FHLSLMaterialFileWatcher());
	Watcher->FilesToWatch = TSet<FString>(InFilesToWatch);

	TSet<FString> Directories;
	for (const FString& File : Watcher->FilesToWatch)
	{
		ensure(File == FPaths::ConvertRelativePathToFull(File));
		Directories.Add(FPaths::GetPath(File));
	}

	const IDirectoryWatcher::FDirectoryChanged Callback = IDirectoryWatcher::FDirectoryChanged::CreateSP(&Watcher.Get(), &FHLSLMaterialFileWatcher::OnDirectoryChanged);
	for (const FString& Directory : Directories)
	{
		Watcher->Watchers.Add(FWatcher::Create(Directory, Callback));
	}

	return Watcher;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FHLSLMaterialFileWatcher::FWatcher> FHLSLMaterialFileWatcher::FWatcher::Create(const FString& Directory, const IDirectoryWatcher::FDirectoryChanged& Callback)
{
	if (Directory.IsEmpty() || 
		!FPaths::DirectoryExists(Directory))
	{
		return nullptr;
	}
	
	FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (!ensure(Module))
	{
		return nullptr;
	}
	IDirectoryWatcher* DirectoryWatcher = Module->Get();
	if (!ensure(DirectoryWatcher))
	{
		return nullptr;
	}

	FDelegateHandle NewDelegateHandle;
	if (!ensure(DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(Directory, Callback, NewDelegateHandle)))
	{
		return nullptr;
	}

	UE_LOG(LogHLSLMaterial, Log, TEXT("Watching directory %s"), *Directory);

	return MakeShareable(new FWatcher(Directory, NewDelegateHandle));
}

FHLSLMaterialFileWatcher::FWatcher::~FWatcher()
{
	FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (!ensure(Module))
	{
		return;
	}
	IDirectoryWatcher* DirectoryWatcher = Module->Get();
	if (!ensure(DirectoryWatcher))
	{
		return;
	}

	ensure(DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Directory, DelegateHandle));

	UE_LOG(LogHLSLMaterial, Log, TEXT("Stopped watching directory %s"), *Directory);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FHLSLMaterialFileWatcher::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges) const
{
	for (const FFileChangeData& FileChange : FileChanges)
	{
		const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FileChange.Filename);
		if (FilesToWatch.Contains(AbsolutePath))
		{
			UE_LOG(LogHLSLMaterial, Log, TEXT("Update triggered from %s"), *AbsolutePath);
			OnFileChanged.Broadcast();
			break;
		}
	}
}