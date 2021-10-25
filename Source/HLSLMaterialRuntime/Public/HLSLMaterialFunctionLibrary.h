// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "HLSLMaterialFunctionLibrary.generated.h"

struct FFileChangeData;
class UMaterialFunction;

UCLASS()
class HLSLMATERIALRUNTIME_API UHLSLMaterialFunctionLibrary : public UObject
{
	GENERATED_BODY()
		
public:
#if WITH_EDITORONLY_DATA
	// HLSL file containing functions
	UPROPERTY(EditAnywhere, Category = "Config")
	FFilePath File;

	// If true assets will automatically be updated when the file is modified on disk by an external editor
	UPROPERTY(EditAnywhere, Category = "Config", AssetRegistrySearchable)
	bool bUpdateOnFileChange = true;

	UPROPERTY(VisibleAnywhere, Category = "Generated")
	TArray<TSoftObjectPtr<UMaterialFunction>> MaterialFunctions;
#endif

#if WITH_EDITOR
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdate, UHLSLMaterialFunctionLibrary&);
	static FOnUpdate OnUpdate;

	void Update()
	{
		OnUpdate.Broadcast(*this);
	}

	FString GetFilePath() const;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	//~ End UObject Interface

private:
	FString WatchedDirectory;
	FDelegateHandle WatcherDelegate;

	void BindWatcher();
	void UnbindWatcher();

	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
#endif
};