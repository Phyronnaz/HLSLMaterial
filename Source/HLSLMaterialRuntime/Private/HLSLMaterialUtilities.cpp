// Copyright 2021 Phyronnaz

#include "HLSLMaterialUtilities.h"
#include "Containers/Ticker.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY(LogHLSLMaterial);

void FHLSLMaterialUtilities::DelayedCall(TFunction<void()> Call, float Delay)
{
	check(IsInGameThread());

	UE_500_SWITCH(FTicker, FTSTicker)::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([=](float)
	{
		Call();
		return false;
	}), Delay);
}

FString FHLSLMaterialUtilities::HashString(const FString& String)
{
	uint32 Hash[5];
	const TArray<TCHAR>& Array = String.GetCharArray();
	FSHA1::HashBuffer(Array.GetData(), Array.Num() * Array.GetTypeSize(), reinterpret_cast<uint8*>(Hash));

	return FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]).ToString();
}