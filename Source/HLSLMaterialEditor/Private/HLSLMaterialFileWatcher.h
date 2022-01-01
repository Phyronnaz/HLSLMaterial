// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "IDirectoryWatcher.h"
#include "Containers/Ticker.h"
#include "HLSLMaterialUtilities.h"

class FHLSLMaterialFileWatcher
	: public FVirtualDestructor
	, public TSharedFromThis<FHLSLMaterialFileWatcher>
	, public UE_500_SWITCH(FTickerObjectBase, FTSTickerObjectBase)
{
public:
	FSimpleMulticastDelegate OnFileChanged;

	static TSharedRef<FHLSLMaterialFileWatcher> Create(const TArray<FString>& InFilesToWatch);

protected:
	//~ Begin FTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override;
	//~ End FTickerObjectBase Interface

private:
	class FWatcher
	{
	public:
		static TSharedPtr<FWatcher> Create(const FString& Directory, const IDirectoryWatcher::FDirectoryChanged& Callback);
		~FWatcher();

	private:
		const FString Directory;
		const FDelegateHandle DelegateHandle;

		FWatcher(const FString& Directory, const FDelegateHandle& DelegateHandle)
			: Directory(Directory)
			, DelegateHandle(DelegateHandle)
		{
		}
	};

	TSet<FString> FilesToWatch;
	TArray<TSharedPtr<FWatcher>> Watchers;

	bool bUpdateOnNextTick = false;

	FHLSLMaterialFileWatcher() = default;

	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
};