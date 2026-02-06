// Copyright 2024 Sabre Dart Studios


#include "Game/HWGameInstance.h"
#include "Modules/ModuleManager.h"


void UHWGameInstance::Init()
{
    Super::Init();

    FModuleManager::Get().LoadModuleChecked("GameplayAbilities");
    FModuleManager::Get().LoadModuleChecked("GameplayTags");
    FModuleManager::Get().LoadModuleChecked("GameplayTasks");
    FModuleManager::Get().LoadModuleChecked("OWSPlugin");
}