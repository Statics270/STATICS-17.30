#pragma once
#include "framework.h"
#include "QuestSystem.h"

namespace BuildingActor {
    void (*OnDamageServerOG)(ABuildingActor* This, float Damage, FGameplayTagContainer& DamageTags, FVector& Momentum, FHitResult& HitInfo, AController* InstigatedBy, AActor* DamageCauser, FGameplayEffectContextHandle& EffectContext);
    void OnDamageServer(ABuildingActor* This, float Damage, FGameplayTagContainer& DamageTags, FVector& Momentum, FHitResult& HitInfo, AController* InstigatedBy, AActor* DamageCauser, FGameplayEffectContextHandle& EffectContext) {
        if (!This || !InstigatedBy || !InstigatedBy->IsA(AFortPlayerControllerAthena::StaticClass()) || !DamageCauser->IsA(AFortWeapon::StaticClass()) || !((AFortWeapon*)DamageCauser)->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) || This->bPlayerPlaced) {
            return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
        }

        ABuildingSMActor* BuildingSMActor = (ABuildingSMActor*)This;
        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)InstigatedBy;
        if (!PC->Pawn) {
            return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
        }
        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PC->Pawn;

        // Clear any blocking states BEFORE harvesting to ensure pickaxe works
        if (Pawn->AbilitySystemComponent && Globals::bHarvestingFix) {
            Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }

        int MaterialCount = (Damage / (UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(6, 12)));

        PC->ClientReportDamagedResourceBuilding(BuildingSMActor, BuildingSMActor->ResourceType, MaterialCount, BuildingSMActor->bDestroyed, (Damage == 100.f));

        UFortResourceItemDefinition* ResourceItemDefinition = UFortKismetLibrary::K2_GetResourceItemDefinition(BuildingSMActor->ResourceType);
        if (!ResourceItemDefinition) {
            return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
        }

        // Fix resource pickup - ensure proper location and pickup handling
        FVector PickupLocation = Pawn->K2_GetActorLocation() + Pawn->GetActorForwardVector() * 100.f;

        AFortPickup* Pickup = SpawnPickup(ResourceItemDefinition, MaterialCount, 0, PickupLocation, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, true, PC->MyFortPawn);

        // Clear blocking states again before pickup
        if (Pawn->AbilitySystemComponent && Globals::bHarvestingFix) {
            Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }

        Pawn->ServerHandlePickup(Pickup, 0.3f, FVector(), true);

        // Track harvesting for quest system
        QuestSystem::OnHarvesting(PC);
        QuestSystem::UpdateQuestProgress(PC, QuestSystem::EQuestObjectiveType::Harvesting, MaterialCount);

        return OnDamageServerOG(This, Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, EffectContext);
    }

    void HookAll() {
        MH_CreateHook((LPVOID)(ImageBase + 0x515FEA4), OnDamageServer, (LPVOID*)&OnDamageServerOG);

        Log("BuildingActor Hooked!");
    }
}