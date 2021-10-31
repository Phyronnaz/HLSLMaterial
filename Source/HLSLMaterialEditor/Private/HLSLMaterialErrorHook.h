// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

class IMaterialEditor;
class FMessageLogListingViewModel;

class FHLSLMaterialErrorHook
{
public:
	static constexpr const TCHAR* PathPrefix = TEXT("[HLSLMaterial]");
	static constexpr const TCHAR* PathSuffix = TEXT("[/HLSLMaterial]");

	static void Register();

private:
	static void HookMessageLogHack(IMaterialEditor& MaterialEditor);
	static void ReplaceMessages(FMessageLogListingViewModel& ViewModel);
};