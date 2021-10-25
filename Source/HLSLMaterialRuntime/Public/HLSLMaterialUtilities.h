// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

struct HLSLMATERIALRUNTIME_API FHLSLMaterialUtilities
{
	// Delay until next fire; 0 means "next frame"
	static void DelayedCall(TFunction<void()> Call, float Delay = 0);
};

HLSLMATERIALRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogHLSLMaterial, Log, All);

#define ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

#define HLSL_STARTUP_FUNCTION(Phase, ...) \
	static const FDelayedAutoRegisterHelper ANONYMOUS_VARIABLE(DelayedAutoRegisterHelper)(Phase, __VA_ARGS__);