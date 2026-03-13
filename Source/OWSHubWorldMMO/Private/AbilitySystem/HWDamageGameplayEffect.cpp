// Copyright 2022 Sabre Dart Studios


#include "./AbilitySystem/HWDamageGameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UHWDamageGameplayEffect::UHWDamageGameplayEffect()
{
	UTargetTagRequirementsGameplayEffectComponent* TagReqComp = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetTagReqs"));
	TagReqComp->ApplicationTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Combat.State.IFrame")));
	GEComponents.Add(TagReqComp);
}