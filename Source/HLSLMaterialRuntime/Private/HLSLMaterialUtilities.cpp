// Copyright 2021 Phyronnaz

#include "HLSLMaterialUtilities.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY(LogHLSLMaterial);

void FHLSLMaterialUtilities::DelayedCall(TFunction<void()> Call, float Delay)
{
	check(IsInGameThread());

	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([=](float)
	{
		Call();
		return false;
	}), Delay);
}