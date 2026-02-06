// Copyright 2024 Sabre Dart Studios

#pragma once

#include "CoreMinimal.h"
#include "OWSGameInstance.h"
#include "HWGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class OWSHUBWORLDMMO_API UHWGameInstance : public UOWSGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	
	
};
