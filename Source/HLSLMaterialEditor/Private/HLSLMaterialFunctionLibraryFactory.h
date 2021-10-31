// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "HLSLMaterialFunctionLibrary.h"
#include "HLSLMaterialFunctionLibraryFactory.generated.h"

UCLASS()
class UHLSLMaterialFunctionLibraryFactory : public UFactory
{
	GENERATED_BODY()

public:
	UHLSLMaterialFunctionLibraryFactory()
	{
		bCreateNew = true;
		bEditAfterNew = true;
		SupportedClass = UHLSLMaterialFunctionLibrary::StaticClass();
	}

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
	{
		return NewObject<UObject>(InParent, Class, Name, Flags);
	}
	//~ End UFactory Interface
};