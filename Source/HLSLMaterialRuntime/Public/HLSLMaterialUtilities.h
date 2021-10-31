// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

struct HLSLMATERIALRUNTIME_API FHLSLMaterialUtilities
{
	// Delay until next fire; 0 means "next frame"
	static void DelayedCall(TFunction<void()> Call, float Delay = 0);

	static FString HashString(const FString& String);
};

HLSLMATERIALRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogHLSLMaterial, Log, All);

#define ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

#define HLSL_STARTUP_FUNCTION(Phase, ...) \
	static const FDelayedAutoRegisterHelper ANONYMOUS_VARIABLE(DelayedAutoRegisterHelper)(Phase, __VA_ARGS__);

template<class T>
struct THLSLRemoveConst
{
	typedef T Type;
};
template<class T>
struct THLSLRemoveConst<const T>
{
	typedef T Type;
};
template<class T>
struct THLSLRemoveConst<const T*>
{
	typedef T* Type;
};
template<class T>
struct THLSLRemoveConst<const T&>
{
	typedef T& Type;
};

#define HLSL_CONST_CAST(X) const_cast<THLSLRemoveConst<decltype(X)>::Type>(X)