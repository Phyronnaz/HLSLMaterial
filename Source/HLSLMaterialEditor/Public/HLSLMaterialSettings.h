// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "HLSLMaterialSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings)
class HLSLMATERIALEDITOR_API UHLSLMaterialSettings : public UObject
{
    GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Config")
		FString Help =
		"For Visual Studio Code, use \n%localappdata%/Programs/Microsoft VS Code/Code.exe\n-g \"%FILE%:%LINE%:%CHAR%\"\n\n"
		"For Rider, use \nC:/Program Files/JetBrains/Rider for Unreal Engine 2021.2/bin/rider64.exe\n--line %LINE% --column %CHAR% \"%FILE%\"";

	// The exe to use to open HLSL files
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (DisplayName = "HLSL Editor"))
	FFilePath HLSLEditor = { "%localappdata%/Programs/Microsoft VS Code/Code.exe" };

	// The arguments to forward to the editor
	// %FILE% is replaced by the full path to the file
	// %LINE% by the line
	// %CHAR% by the char/column
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (DisplayName = "HLSL Editor Args"))
	FString HLSLEditorArgs = "-g \"%FILE%:%LINE%:%CHAR%\"";

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);

		SaveConfig();
	}
};