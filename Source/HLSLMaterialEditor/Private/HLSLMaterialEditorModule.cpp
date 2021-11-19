// Copyright 2021 Phyronnaz

#include "CoreMinimal.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "Modules/ModuleInterface.h"
#include "HLSLMaterialSettings.h"
#include "HLSLMaterialFunctionLibrary.h"

class FAssetTypeActions_HLSLMaterialFunctionLibrary : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_HLSLMaterialFunctionLibrary() = default;

	//~ Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return INVTEXT("HLSL Material Function Library"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 175, 255); }
	virtual UClass* GetSupportedClass() const override { return UHLSLMaterialFunctionLibrary::StaticClass(); }
	virtual uint32 GetCategories() override
	{
#if ENGINE_MAJOR_VERSION >= 5
		// We want to support both 5.0EA and 5.0 release
		// As far as I found they have exactly the same version values in defines so we cannot check for that.
		// 5.0 release added EAssetTypeCategories::Materials but EA still has MaterialsAndTextures category
		// They both have the same numerical value in the enum so a hacky solution for now is just to use raw value.
		return 1 << 2;
#else
		return EAssetTypeCategories::MaterialsAndTextures;
#endif
	}

	virtual bool HasActions(const TArray<UObject*>&InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>&InObjects, FMenuBuilder & MenuBuilder) override
	{
		MenuBuilder.AddMenuEntry(
			INVTEXT("Update from HLSL"),
			INVTEXT("Update all the generated material functions from the HLSL code"),
			{},
			FUIAction(FExecuteAction::CreateLambda([this, Assets = GetTypedWeakObjectPtrs<UHLSLMaterialFunctionLibrary>(InObjects)]()
		{
			for (const TWeakObjectPtr<UHLSLMaterialFunctionLibrary>& Asset : Assets)
			{
				if (ensure(Asset.IsValid()))
				{
					IHLSLMaterialEditorInterface::Get().Update(*Asset);
				}
			}
		})));
	}
	//~ End IAssetTypeActions Interface
};

class FHLSLMaterialEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_HLSLMaterialFunctionLibrary>());
		
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings(
			"Editor",
			"Plugins",
			"HLSL Material",
			INVTEXT("HLSL Material"),
			INVTEXT("Settings related to the HLSL Material plugin."),
			GetMutableDefault<UHLSLMaterialSettings>());
	}
};
IMPLEMENT_MODULE(FHLSLMaterialEditorModule, HLSLMaterialEditor);