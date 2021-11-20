// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialShaderInfo.generated.h"

#define ENABLE_PERMUTATION_WINDOW (ENGINE_VERSION == 427)

class IMaterialEditor;

UCLASS(Transient, Within=MaterialInterface)
class UHLSLMaterialShaderInfoLayout : public UObject
{
	GENERATED_BODY()

public:
	FVertexFactoryType* VertexFactoryType = nullptr;
	FShaderType* ShaderType = nullptr;
	FShaderPipelineType* ShaderPipelineType = nullptr;

public:
	UPROPERTY(VisibleAnywhere, Category = Stats)
	FName VertexFactory;

	UPROPERTY(VisibleAnywhere, Category = Stats)
	FName Shader;

	UPROPERTY(VisibleAnywhere, Category = Stats)
	int32 PermutationId = 0;

	UPROPERTY(VisibleAnywhere, Category = Stats)
	FName ShaderPipeline;

	UPROPERTY(EditAnywhere, Category = Stats)
	bool bGenerateHLSL = false;

	void Build()
	{
		if (VertexFactoryType)
		{
			VertexFactory = VertexFactoryType->GetFName();
		}
		if (ShaderType)
		{
			Shader = ShaderType->GetFName();
		}
		if (ShaderPipelineType)
		{
			ShaderPipeline = ShaderPipelineType->GetFName();
		}
	}
	
#if ENABLE_PERMUTATION_WINDOW
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

#if ENABLE_PERMUTATION_WINDOW
class FHLSLMaterialShaderInfo
{
public:
	static constexpr EShaderPlatform Platform = SP_PCD3D_SM5;

	struct FShaderLayout
	{
		FShaderType* ShaderType;
		int32 PermutationId;
	};
	struct FLayout
	{
		TMap<FVertexFactoryType*, TArray<FShaderLayout>> ShaderLayouts;
		TMap<FVertexFactoryType*, TArray<FShaderPipelineType*>> ShaderPipelines;
	};

	static void Initialize();
	static void SetupMaterialEditor(IMaterialEditor& MaterialEditor);
	static FLayout GetLayout(const FMaterial& Material);
};
#endif