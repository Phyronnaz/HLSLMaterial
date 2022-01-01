// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Launch/Resources/Version.h"

struct HLSLMATERIALRUNTIME_API FHLSLMaterialUtilities
{
	// Delay until next fire; 0 means "next frame"
	static void DelayedCall(TFunction<void()> Call, float Delay = 0);

	static FString HashString(const FString& String);
};

HLSLMATERIALRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogHLSLMaterial, Log, All);

#ifndef HLSL_MATERIAL_UE5_EA
#define HLSL_MATERIAL_UE5_EA 0
#endif

#if HLSL_MATERIAL_UE5_EA
#define ENGINE_VERSION 499
#else
#define ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)
#endif

#if ENGINE_VERSION >= 500 && !HLSL_MATERIAL_UE5_EA
#define UE_500_SWITCH(Before, AfterOrEqual) AfterOrEqual
#define UE_500_ONLY(...) __VA_ARGS__
#else
#define UE_500_SWITCH(Before, AfterOrEqual) Before
#define UE_500_ONLY(...)
#endif

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