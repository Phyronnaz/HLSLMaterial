// Copyright 2021 Phyronnaz

#include "HLSLMaterialShaderInfo.h"
#include "HLSLMaterialUtilities.h"
#include "IMaterialEditor.h"
#include "IPropertyTable.h"
#include "MaterialEditorModule.h"
#include "DesktopPlatformModule.h"
#include "MaterialShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "MeshMaterialShaderType.h"
#include "ShaderCompiler.h"
#include "Containers/LazyPrintf.h"
#include "Misc/FileHelper.h"
#include "UObject/StrongObjectPtr.h"

bool FMaterialShaderType::ShouldCompilePermutation(EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, int32 PermutationId, EShaderPermutationFlags Flags) const
{
	return FShaderType::ShouldCompilePermutation(FMaterialShaderPermutationParameters(Platform, MaterialParameters, PermutationId, Flags));
}

bool FMaterialShaderType::ShouldCompilePipeline(const FShaderPipelineType* ShaderPipelineType, EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, EShaderPermutationFlags Flags)
{
	const FMaterialShaderPermutationParameters Parameters(Platform, MaterialParameters, kUniqueShaderPermutationId, Flags);
	for (const FShaderType* ShaderType : ShaderPipelineType->GetStages())
	{
		checkSlow(ShaderType->GetMaterialShaderType());
		if (!ShaderType->ShouldCompilePermutation(Parameters))
		{
			return false;
		}
	}
	return true;
}

bool FMeshMaterialShaderType::ShouldCompilePermutation(EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, const FVertexFactoryType* VertexFactoryType, int32 PermutationId, EShaderPermutationFlags Flags) const
{
	return FShaderType::ShouldCompilePermutation(FMeshMaterialShaderPermutationParameters(Platform, MaterialParameters, VertexFactoryType, PermutationId, Flags));
}

bool FMeshMaterialShaderType::ShouldCompileVertexFactoryPermutation(const FVertexFactoryType* VertexFactoryType, EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, EShaderPermutationFlags Flags)
{
	return VertexFactoryType->ShouldCache(FVertexFactoryShaderPermutationParameters(Platform, MaterialParameters, VertexFactoryType, Flags));
}

bool FMeshMaterialShaderType::ShouldCompilePipeline(const FShaderPipelineType* ShaderPipelineType, EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, const FVertexFactoryType* VertexFactoryType, EShaderPermutationFlags Flags)
{
	const FMeshMaterialShaderPermutationParameters Parameters(Platform, MaterialParameters, VertexFactoryType, kUniqueShaderPermutationId, Flags);
	for (const FShaderType* ShaderType : ShaderPipelineType->GetStages())
	{
		checkSlow(ShaderType->GetMeshMaterialShaderType());
		if (!ShaderType->ShouldCompilePermutation(Parameters))
		{
			return false;
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FHLSLMaterialShaderInfo::Initialize()
{
	IMaterialEditorModule& MaterialEditorModule = FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	MaterialEditorModule.OnMaterialEditorOpened().AddLambda([](TWeakPtr<IMaterialEditor> WeakMaterialEditor)
	{
		SetupMaterialEditor(*WeakMaterialEditor.Pin());
	});
}

void FHLSLMaterialShaderInfo::SetupMaterialEditor(IMaterialEditor& MaterialEditor)
{
	MaterialEditor.OnRegisterTabSpawners().AddLambda([&](const TSharedRef<FTabManager>& TabManager)
	{
		TabManager->RegisterTabSpawner("PermutationTabId", FOnSpawnTab::CreateLambda([&, KeepAliveArray = MakeShared<TArray<TStrongObjectPtr<UObject>>>()](const FSpawnTabArgs& Args)
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

			UMaterialInterface* MaterialInterface = MaterialEditor.GetMaterialInterface();
			const FMaterialResource* Material = MaterialInterface->GetMaterialResource(ERHIFeatureLevel::SM5);
			const FLayout Layout = GetLayout(*Material);

			const TSharedRef<IPropertyTable> PropertyTable = PropertyEditorModule.CreatePropertyTable();
			PropertyTable->SetSelectionMode(ESelectionMode::None);
			PropertyTable->SetIsUserAllowedToChangeRoot(false);
			PropertyTable->SetOrientation(EPropertyTableOrientation::AlignPropertiesInColumns);
			PropertyTable->SetShowObjectName(false);

			TArray<UObject*> LayoutObjects;

			for (auto& It : Layout.ShaderLayouts)
			{
				for (auto& ShaderIt : It.Value)
				{
					UHLSLMaterialShaderInfoLayout* LayoutObject = NewObject<UHLSLMaterialShaderInfoLayout>(MaterialInterface);
					LayoutObject->VertexFactoryType = It.Key;
					LayoutObject->ShaderType = ShaderIt.ShaderType;
					LayoutObject->PermutationId = ShaderIt.PermutationId;
					LayoutObject->Build();

					LayoutObjects.Add(LayoutObject);
				}
			}

			for (auto& It : Layout.ShaderPipelines)
			{
				for (auto& Pipeline : It.Value)
				{
					UHLSLMaterialShaderInfoLayout* LayoutObject = NewObject<UHLSLMaterialShaderInfoLayout>(MaterialInterface);
					LayoutObject->VertexFactoryType = It.Key;
					LayoutObject->ShaderPipelineType = Pipeline;
					LayoutObject->Build();

					LayoutObjects.Add(LayoutObject);
				}
			}

			for (UObject* Object : LayoutObjects)
			{
				KeepAliveArray->Emplace(Object);
			}
			PropertyTable->SetObjects(LayoutObjects);

			for (TFieldIterator<FProperty> It(UHLSLMaterialShaderInfoLayout::StaticClass()); It; ++It)
			{
				PropertyTable->AddColumn(*It);
			}
			PropertyTable->RequestRefresh();

			return
				SNew(SDockTab)
				.Icon(FEditorStyle::GetBrush("Kismet.Tabs.CompilerResults"))
				.Label(INVTEXT("Permutations"))
				[
					PropertyEditorModule.CreatePropertyTableWidget(PropertyTable)
				];
		}))
		.SetDisplayName(INVTEXT("Permutations"))
		.SetGroup(MaterialEditor.GetWorkspaceMenuCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
	});
}

HLSL_STARTUP_FUNCTION(EDelayedRegisterRunPhase::EndOfEngineInit, FHLSLMaterialShaderInfo::Initialize);

 FHLSLMaterialShaderInfo::FLayout FHLSLMaterialShaderInfo::GetLayout(const FMaterial& Material)
{
	FLayout Layout;

	const FMaterialShaderParameters MaterialParameters(&Material);
	const EShaderPermutationFlags Flags = GetCurrentShaderPermutationFlags();

	const bool bHasTessellation = MaterialParameters.TessellationMode != MTM_NoTessellation;
	const TArray<FShaderType*>& SortedMaterialShaderTypes = FShaderType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::Material);
	const TArray<FShaderType*>& SortedMeshMaterialShaderTypes = FShaderType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::MeshMaterial);
	const TArray<FShaderPipelineType*>& SortedMaterialPipelineTypes = FShaderPipelineType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::Material);
	const TArray<FShaderPipelineType*>& SortedMeshMaterialPipelineTypes = FShaderPipelineType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::MeshMaterial);

	for (FShaderType* BaseShaderType : SortedMaterialShaderTypes)
	{
		FMaterialShaderType* ShaderType = static_cast<FMaterialShaderType*>(BaseShaderType);
		const int32 PermutationCount = ShaderType->GetPermutationCount();
		for (int32 PermutationId = 0; PermutationId < PermutationCount; ++PermutationId)
		{
			if (ShaderType->ShouldCompilePermutation(Platform, MaterialParameters, PermutationId, Flags))
			{
				Layout.ShaderLayouts.FindOrAdd(nullptr).Add({ ShaderType, PermutationId });
			}
		}
	}

	if (RHISupportsShaderPipelines(Platform))
	{
		// Iterate over all pipeline types
		for (FShaderPipelineType* ShaderPipelineType : SortedMaterialPipelineTypes)
		{
			if (ShaderPipelineType->HasTessellation() == bHasTessellation &&
				FMaterialShaderType::ShouldCompilePipeline(ShaderPipelineType, Platform, MaterialParameters, Flags))
			{
				Layout.ShaderPipelines.FindOrAdd(nullptr).Add(ShaderPipelineType);
			}
		}
	}

	for (FVertexFactoryType* VertexFactoryType : FVertexFactoryType::GetSortedMaterialTypes())
	{
		if (!FMeshMaterialShaderType::ShouldCompileVertexFactoryPermutation(VertexFactoryType, Platform, MaterialParameters, Flags))
		{
			continue;
		}

		for (FShaderType* BaseShaderType : SortedMeshMaterialShaderTypes)
		{
			FMeshMaterialShaderType* ShaderType = static_cast<FMeshMaterialShaderType*>(BaseShaderType);
			const int32 PermutationCount = ShaderType->GetPermutationCount();
			for (int32 PermutationId = 0; PermutationId < PermutationCount; ++PermutationId)
			{
				if (ShaderType->ShouldCompilePermutation(Platform, MaterialParameters, VertexFactoryType, PermutationId, Flags))
				{
					Layout.ShaderLayouts.FindOrAdd(VertexFactoryType).Add({ ShaderType, PermutationId });
				}
			}
		}

		if (RHISupportsShaderPipelines(Platform))
		{
			for (FShaderPipelineType* ShaderPipelineType : SortedMeshMaterialPipelineTypes)
			{
				if (ShaderPipelineType->HasTessellation() == bHasTessellation &&
					FMeshMaterialShaderType::ShouldCompilePipeline(ShaderPipelineType, Platform, MaterialParameters, VertexFactoryType, Flags))
				{
					Layout.ShaderPipelines.FindOrAdd(VertexFactoryType).Add(ShaderPipelineType);
				}
			}
		}
	}

	return Layout;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UHLSLMaterialShaderInfoLayout::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!bGenerateHLSL)
	{
		return;
	}
	bGenerateHLSL = false;

	if (ShaderPipelineType)
	{
		return;
	}

	UMaterialInterface* MaterialInterface = GetOuterUMaterialInterface();
	FMaterialResource* Material = MaterialInterface->GetMaterialResource(ERHIFeatureLevel::SM5);
	if (!Material)
	{
		return;
	}

	FMaterialShaderParameters MaterialParameters(Material);

	constexpr EShaderPlatform Platform = FHLSLMaterialShaderInfo::Platform;

	FShaderCompilerInput Input;

	VertexFactoryType->ModifyCompilationEnvironment(
		FVertexFactoryShaderPermutationParameters(
			Platform,
			MaterialParameters,
			VertexFactoryType,
			GetCurrentShaderPermutationFlags()),
		Input.Environment);

	ShaderType->ModifyCompilationEnvironment(
		FMeshMaterialShaderPermutationParameters(
			Platform,
			MaterialParameters,
			VertexFactoryType,
			PermutationId,
			GetCurrentShaderPermutationFlags()),
		Input.Environment);

	GlobalBeginCompileShader(
		"",
		VertexFactoryType,
		ShaderType,
		ShaderPipelineType,
		PermutationId,
		TEXT("/Dummy/Dummy.usf"),
		TEXT("Main"),
		FShaderTarget(ShaderType->GetFrequency(), Platform),
		Input
	);
	
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> SaveFilenames;
	DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		"Save HLSL",
		FPaths::ProjectDir(),
		"Intellisense.hlsl",
		"*",
		EFileDialogFlags::None,
		SaveFilenames
	);

	if (SaveFilenames.Num() != 1)
	{
		return;
	}

	Input.Environment.SetDefine(TEXT("SM5_PROFILE"), 1);

	FString Text;
	Text += "// Autogenerated file\n";
	Text += "// Vertex Factory: " + VertexFactory.ToString() + "\n";
	Text += "// Shader: " + Shader.ToString() + "\n";

	if (PermutationId != 0)
	{
		Text += "// Permutation: " + FString::FromInt(PermutationId) + "\n";
	}

	Text += "\n\n";

	for (auto& It : Input.Environment.GetDefinitions())
	{
		Text += "#define " + It.Key + " " + It.Value + "\n";
	}
	Text += "\n\n";

	for (auto& It : Input.Environment.IncludeVirtualPathToExternalContentsMap)
	{
		Text += *It.Value;
		Text += "\n\n";
	}

	FString MaterialTemplate;
	LoadShaderSourceFileChecked(TEXT("/Engine/Private/MaterialTemplate.ush"), Platform, MaterialTemplate);
	FLazyPrintf LazyPrintf(*MaterialTemplate);

	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), 10));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), 10));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), 10));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), 10));

	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));
	LazyPrintf.PushParam(TEXT(""));

	Text += LazyPrintf.GetResultString();

	FFileHelper::SaveStringToFile(Text, *SaveFilenames[0]);
}