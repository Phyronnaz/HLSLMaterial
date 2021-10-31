// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionLibrary.h"

#if WITH_EDITOR
#include "HAL/FileManagerGeneric.h"
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

	// Try to convert from /Game/Smthg to the full filename
	if (!FPackageName::TryConvertLongPackageNameToFilename(InFilePath, FullPath))
	{
		FullPath = InFilePath;
	}

	// Always return the full path
	return FPaths::ConvertRelativePathToFull(InFilePath);
}

void UHLSLMaterialFunctionLibrary::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.MemberProperty)
	{
		return;
	}

	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty->GetFName();

	if (!(MemberPropertyName != GET_MEMBER_NAME_CHECKED(UHLSLMaterialFunctionLibrary, File) ||
		  MemberPropertyName != GET_MEMBER_NAME_CHECKED(UHLSLMaterialFunctionLibrary, IncludeFilePaths)))
	{
		return;
	}

	UnbindWatchers();
	if (bUpdateOnFileChange)
	{
		BindWatchers();
	}

	FString NewPath = File.FilePath;
	MakeRelativePath(NewPath);

	if (GetFilePath(File.FilePath) ==
		GetFilePath(NewPath))
	{
		// Conversion is safe
		File.FilePath = NewPath;
	}
}

void UHLSLMaterialFunctionLibrary::BeginDestroy()
{
	Super::BeginDestroy();

	UnbindWatchers();
}

void UHLSLMaterialFunctionLibrary::PostLoad()
{
	Super::PostLoad();

	if (bUpdateOnFileChange)
	{
		BindWatchers();
	}
}

void BindWatcher(const FString& FileDirectory, FString& WatchedDirectory, FDelegateHandle& WatcherDelegate, const IDirectoryWatcher::FDirectoryChanged& Callback)
{
	if (WatcherDelegate.IsValid())
	{
		return;
	}

	ensure(WatchedDirectory.IsEmpty());

	WatchedDirectory = FileDirectory;

	if (WatchedDirectory.IsEmpty() || 
		!FPaths::DirectoryExists(WatchedDirectory))
	{
		WatchedDirectory.Reset();
		return;
	}

	FDirectoryWatcherModule& Module = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (IDirectoryWatcher* DirectoryWatcher = Module.Get())
	{
		ensure(DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(WatchedDirectory, Callback, WatcherDelegate));
	}
}

void UnbindWatcher(FString& WatchedDirectory, FDelegateHandle& WatcherDelegate)
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

void UHLSLMaterialFunctionLibrary::BindWatchers()
{
	BindWatcher(
		FPaths::GetPath(GetFilePath()),
		Watched.Directory,
		Watched.Handle,
		IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UHLSLMaterialFunctionLibrary::OnDirectoryChanged));

	TSet<FString> UniqueIncludeMappedPaths;
	for (const FString& IncludeFilePath : IncludeFilePaths)
	{
		if (IncludeFilePath.IsEmpty())
		{
			continue;
		}
		
		const FString IncludePath = FPaths::GetPath(IncludeFilePath);

		const FString* IncludeMappedPath = AllShaderSourceDirectoryMappings().Find(IncludePath);

		if (!ensureAlwaysMsgf(IncludeMappedPath, TEXT("Failed to find shader mapping path for %s"), *IncludePath))
		{
			continue;
		}

		UniqueIncludeMappedPaths.Add(*IncludeMappedPath);
	}

	ensure(WatchedIncludes.Num() == 0);

	for (const FString& IncludeMappedPath : UniqueIncludeMappedPaths)
	{
		FWatched& WatchedInclude = WatchedIncludes.AddDefaulted_GetRef();

		BindWatcher(
			*IncludeMappedPath,
			WatchedInclude.Directory,
			WatchedInclude.Handle,
			IDirectoryWatcher::FDirectoryChanged::CreateUObject(
				this, &UHLSLMaterialFunctionLibrary::OnIncludeDirectoryChanged));
	}
}

void UHLSLMaterialFunctionLibrary::UnbindWatchers()
{
	UnbindWatcher(Watched.Directory, Watched.Handle);
	
	for (FWatched& WatchedInclude : WatchedIncludes)
	{
		UnbindWatcher(WatchedInclude.Directory, WatchedInclude.Handle);
	}
	WatchedIncludes.Reset();
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

void UHLSLMaterialFunctionLibrary::OnIncludeDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	for (const FFileChangeData& FileChange : FileChanges)
	{
		FString FileChangePath = FPaths::GetPath(FileChange.Filename);
		
		// Paths in ShaderSourceDirectoryMapping are relative
		FileChangePath = FFileManagerGeneric::DefaultConvertToRelativePath(*FileChangePath);
		
		const FString* MappedFileChangePath = AllShaderSourceDirectoryMappings().FindKey(FileChangePath);

		if (!MappedFileChangePath)
		{
			continue;
		}

		const FString FileChangeFilename = FPaths::GetCleanFilename(FileChange.Filename);
		const FString MappedFileChangeFilePath = *MappedFileChangePath  / *FileChangeFilename;

		if (IncludeFilePaths.Contains(MappedFileChangeFilePath))
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