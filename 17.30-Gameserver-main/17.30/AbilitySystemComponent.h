#pragma once
#include "framework.h"

namespace AbilitySystemComponent {
    void GiveAbility(UFortGameplayAbility* Ability, AFortPlayerState* PS)
    {
        if (!Ability || !PS) {
            return;
        }

        FGameplayAbilitySpec Spec{};
        AbilitySpecConstructor(&Spec, Ability, 1, -1, nullptr);
        GiveAbilityOG(PS->AbilitySystemComponent, &Spec.Handle, Spec);
    }

    void GiveAbilitySet(UFortAbilitySet* AbilitySet, AFortPlayerState* PS)
    {
        if (!AbilitySet || !PS) {
            std::string abilitySetName = AbilitySet ? AbilitySet->GetFullName() : "None";
            std::string playerStateName = PS ? PS->GetFullName() : "None";
            Log("Cannot Give AbilitySet! NULL!");
            Log(std::string("AbilitySet: ") + abilitySetName);
            Log(std::string("PlayerState: ") + playerStateName);
            return;
        }

        for (int i = 0; i < AbilitySet->GameplayAbilities.Num(); i++) {
            GiveAbility((UFortGameplayAbility*)AbilitySet->GameplayAbilities[i].Get()->DefaultObject, PS);
        }

        for (int i = 0; i < AbilitySet->GrantedGameplayEffects.Num(); i++) {
            UClass* GameplayEffect = AbilitySet->GrantedGameplayEffects[i].GameplayEffect.Get();
            float Level = AbilitySet->GrantedGameplayEffects[i].Level;

            if (!GameplayEffect)
                continue;

            PS->AbilitySystemComponent->BP_ApplyGameplayEffectToSelf(GameplayEffect, Level, FGameplayEffectContextHandle());
        }
    }

    //https://github.com/EpicGames/UnrealEngine/blob/87f8792983fb4228be213b15b57f675dfe143d16/Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent_Abilities.cpp#L584
    FGameplayAbilitySpec* FindAbilitySpecFromHandle(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle)
    {
        for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->ActivatableAbilities.Items)
        {
            if (Spec.Handle.Handle == Handle.Handle)
                return &Spec;
        }

        return nullptr;
    }

    //https://github.com/EpicGames/UnrealEngine/blob/87f8792983fb4228be213b15b57f675dfe143d16/Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent_Abilities.cpp#L1445
    void InternalServerTryActiveAbility(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool InputPressed, const FPredictionKey& PredictionKey, const FGameplayEventData* TriggerEventData)
    {
        FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilitySystemComponent, Handle);
        if (!Spec)
        {
            AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
            return;
        }

        const UGameplayAbility* AbilityToActivate = Spec->Ability;

        AActor* OwnerActor = AbilitySystemComponent->GetOwner();
        AFortPlayerControllerAthena* PC = nullptr;

        if (OwnerActor)
        {
            if (OwnerActor->IsA(AFortPlayerControllerAthena::StaticClass()))
            {
                PC = (AFortPlayerControllerAthena*)OwnerActor;
            }
            else if (OwnerActor->IsA(AFortPlayerPawnAthena::StaticClass()))
            {
                AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)OwnerActor;
                PC = Pawn ? (AFortPlayerControllerAthena*)Pawn->Controller : nullptr;
            }
            else if (OwnerActor->IsA(AFortPlayerStateAthena::StaticClass()))
            {
                AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)OwnerActor;
                PC = PlayerState ? (AFortPlayerControllerAthena*)PlayerState->GetOwner() : nullptr;
            }
        }

        // Only block clearly aircraft-specific abilities while in aircraft
        if (PC && PC->IsInAircraft())
        {
            std::string AbilityName = AbilityToActivate ? AbilityToActivate->GetFullName() : "";
            if (AbilityToActivate)
            {
                bool bShouldBlock = AbilityName.find("Aircraft") != std::string::npos ||
                    AbilityName.find("SkyDive") != std::string::npos ||
                    AbilityName.find("Skydiv") != std::string::npos ||
                    AbilityName.find("Glider") != std::string::npos ||
                    AbilityName.find("Parachute") != std::string::npos ||
                    AbilityName.find("Bus") != std::string::npos;

                if (bShouldBlock)
                {
                    AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
                    return;
                }
            }
        }

        // Clear any blocking states before attempting activation
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }

        UGameplayAbility* InstancedAbility = nullptr;
        Spec->InputPressed = InputPressed;

        if (!InternalTryActivateAbility(AbilitySystemComponent, Handle, PredictionKey, &InstancedAbility, nullptr, TriggerEventData))
        {
            AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
            Spec->InputPressed = false;

            AbilitySystemComponent->ActivatableAbilities.MarkItemDirty(*Spec);
        }
    }

    // Hook to fix consumable blocking
    void (*ConsumeItemOG)(AFortPlayerControllerAthena* PC, FFortItemEntry& ItemEntry);
    void ConsumeItem(AFortPlayerControllerAthena* PC, FFortItemEntry& ItemEntry)
    {
        if (!PC || !PC->Pawn)
            return ConsumeItemOG(PC, ItemEntry);

        // Ensure we're not blocking consumables unnecessarily
        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PC->Pawn;

        // Reset any blocked state flags by clearing active gameplay effects
        if (Pawn->AbilitySystemComponent)
        {
            Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }

        return ConsumeItemOG(PC, ItemEntry);
    }

    void HookAll()
    {
        int InternalServerTryActiveAbilityIndex = 0xFE;

        HookVTable(UAbilitySystemComponent::GetDefaultObj(), InternalServerTryActiveAbilityIndex, InternalServerTryActiveAbility, nullptr);
        HookVTable(UFortAbilitySystemComponent::GetDefaultObj(), InternalServerTryActiveAbilityIndex, InternalServerTryActiveAbility, nullptr);
        HookVTable(UFortAbilitySystemComponentAthena::GetDefaultObj(), InternalServerTryActiveAbilityIndex, InternalServerTryActiveAbility, nullptr);

        // Hook consumable usage to prevent blocking
        MH_CreateHook((LPVOID)(ImageBase + 0x48F5F80), ConsumeItem, (LPVOID*)&ConsumeItemOG);

        Log("AbilitySystemComponent Hooked!");
    }
}