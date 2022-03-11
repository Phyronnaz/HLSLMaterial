// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustom.h"
#include "HLSLMaterialFunctionLibrary.generated.h"

class UHLSLMaterialFunctionLibrary;

#if WITH_EDITOR
class HLSLMATERIALRUNTIME_API IHLSLMaterialEditorInterface
{
public:
	virtual ~IHLSLMaterialEditorInterface() = default;

	virtual TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLMaterialFunctionLibrary& Library) = 0;
	virtual void Update(UHLSLMaterialFunctionLibrary& Library) = 0;

public:
	static IHLSLMaterialEditorInterface* Get()
	{
		return StaticInterface;
	}

private:
	static IHLSLMaterialEditorInterface* StaticInterface;

	friend class FHLSLMaterialFunctionLibraryEditor;
};
#endif

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

	// Update the assets when any of the included files are updated
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bUpdateOnIncludeChange = false;

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

	UPROPERTY(EditAnywhere, Category = "Config")
	bool bAutomaticallyApply = true;

	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<FText> Categories = { NSLOCTEXT("MaterialExpression", "Misc", "Misc") };

	UPROPERTY(EditAnywhere, Category = "Generated")
	TArray<TSoftObjectPtr<UMaterialFunction>> MaterialFunctions;
#endif

#if WITH_EDITOR
public:
	FString GetFilePath() const;
	static FString GetFilePath(const FString& InFilePath);

	void CreateWatcherIfNeeded();

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	//~ End UObject Interface

private:
	TSharedPtr<FVirtualDestructor> Watcher;

	static void MakeRelativePath(FString& Path);

public:
	static bool TryConvertShaderPathToFilename(const FString& ShaderPath, FString& OutFilename);
	static bool TryConvertFilenameToShaderPath(const FString& Filename, FString& OutShaderPath);

private:
	static bool TryConvertPathImpl(const TMap<FString, FString>& DirectoryMappings, const FString& InPath, FString& OutPath);
#endif
};