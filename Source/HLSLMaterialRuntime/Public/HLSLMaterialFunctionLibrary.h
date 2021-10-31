// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

#include "Materials/MaterialExpressionCustom.h"
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

	// If true, functions will be put in a folder named "AssetName_GeneratedFunctions"
	// If false they'll be generated next to this asset
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bPutFunctionsInSubdirectory = true;

	// If true, will insert preprocessor directive so that compilation errors are relative to your hlsl file
	// instead of the huge generated material file
	//
	// ie, errors will look like MyFile.hlsl:9 instead of /Generated/Material.usf:2330
	// 
	// The downside is that whenever you add or remove a line to your file, all the functions below it will have to be recompiled
	// If compilation is taking forever for you, consider turning this off
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bAccurateErrors = true;

	// Gets passed to the IncludeFilePaths property in material function custom node
	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<FString> IncludeFilePaths;
	
	// Gets passed to the AdditionalDefines property in material function custom node
	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<FCustomDefine> AdditionalDefines;

	UPROPERTY(EditAnywhere, Category = "Generated")
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
	static FString GetFilePath(const FString& InFilePath);

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	//~ End UObject Interface

private:
	struct FWatched
	{
		FString Directory;
		FDelegateHandle Handle;
	};
	FWatched Watched;
	TArray<FWatched> WatchedIncludes;

	void BindWatchers();
	void UnbindWatchers();

	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges);
	void OnIncludeDirectoryChanged(const TArray<FFileChangeData>& FileChanges);

	static void MakeRelativePath(FString& Path);
#endif
};