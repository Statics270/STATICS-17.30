#pragma once
#include "framework.h"
#include "FortInventory.h"
#include "QuestSystem.h"

namespace FortPlayerPawn {
    void ServerHandlePickup(AFortPlayerPawnAthena* Pawn, AFortPickup* PickUp, float InFlyTime, FVector& InStartDirection, bool bPlayPickupSound) {
        //Log("ServerHandlePickup Called!");
        if (!Pawn || !PickUp) {
            return;
        }
        if (PickUp->bPickedUp) {
            return;
        }

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;

        FFortItemEntry& PickupItemEntry = PickUp->PrimaryPickupItemEntry;

        PickUp->PickupLocationData.PickupGuid = PickUp->PrimaryPickupItemEntry.ItemGuid;
        PickUp->PickupLocationData.PickupTarget = Pawn;
        PickUp->PickupLocationData.ItemOwner = Pawn;
        PickUp->PickupLocationData.FlyTime = 0.4f;
        PickUp->PickupLocationData.bPlayPickupSound = true;
        PickUp->OnRep_PickupLocationData();

        PickUp->bPickedUp = true;
        PickUp->OnRep_bPickedUp();
    }

    __int64 (*CompletePickupAnimationOG)(AFortPickup* Pickup);
    __int64 CompletePickupAnimation(AFortPickup* Pickup)
    {
        //Log("CompletePickupAnimation Called!");
        if (!Pickup) {
            Log("No Pickup!");
            return CompletePickupAnimationOG(Pickup);
        }

        FFortPickupLocationData& PickupLocationData = Pickup->PickupLocationData;
        FFortItemEntry& PickupEntry = Pickup->PrimaryPickupItemEntry;

        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PickupLocationData.PickupTarget;
        if (!Pawn) return CompletePickupAnimationOG(Pickup);

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC) return CompletePickupAnimationOG(Pickup);
        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)PC->PlayerState;
        if (!PlayerState) return CompletePickupAnimationOG(Pickup);
        if (PlayerState->bIsABot) return CompletePickupAnimationOG(Pickup);

        UFortItemDefinition* PickupItemDefinition = PickupEntry.ItemDefinition;

        int PickupCount = PickupEntry.Count;
        int PickupLoadedAmmo = PickupEntry.LoadedAmmo;
        int PickupMaxStackSize = FortInventory::GetMaxStackSize(PickupItemDefinition);
        if (!PC->WorldInventory) return CompletePickupAnimationOG(Pickup);
        FFortItemEntry* ItemEntry = FortInventory::FindItemEntry(PC, PickupItemDefinition);

        AFortWeapon* CurrentWeapon = Pawn->CurrentWeapon;
        if (!CurrentWeapon || !CurrentWeapon->WeaponData) {
            return CompletePickupAnimationOG(Pickup);
        }

        FVector Drop = Pawn->K2_GetActorLocation() + Pawn->GetActorForwardVector() * 100.f;
        if (ItemEntry) {
            if (PickupItemDefinition->IsStackable()) {
                bool bCanPickup = (ItemEntry->Count + PickupCount) <= PickupMaxStackSize;
                //Log(std::to_string((ItemEntry->Count + PickupCount)));
                //Log("PickupMaxStackSize: " + std::to_string(PickupMaxStackSize));
                if (PickupItemDefinition->IsA(UFortTrapItemDefinition::StaticClass())) {
                    bCanPickup = true;
                }
                if (bCanPickup) {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo, true);
                }
                else {
                    int Space = PickupMaxStackSize - ItemEntry->Count;
                    int AddToStack = UKismetMathLibrary::GetDefaultObj()->Min(Space, PickupCount);
                    int LeftOver = PickupCount - AddToStack;

                    if (AddToStack > 0) {
                        FortInventory::GiveItem(PC, PickupItemDefinition, AddToStack, 0, true);
                        SpawnPickup(PickupItemDefinition, LeftOver, PickupLoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                    }
                    else {
                        if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary) {
                            FFortItemEntry* CurrentWeaponItemEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);

                            SpawnPickup(CurrentWeapon->WeaponData, CurrentWeaponItemEntry->Count, CurrentWeaponItemEntry->LoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                            FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0, false);
                        }
                        else {
                            SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                        }
                    }
                }
            }
            else {
                if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary) {
                    if (FortInventory::IsInventoryFull(PC)) {
                        FFortItemEntry* CurrentWeaponItemEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);

                        SpawnPickup(CurrentWeapon->WeaponData, CurrentWeaponItemEntry->Count, CurrentWeaponItemEntry->LoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                        FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                    }
                    else {
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                    }
                }
                else {
                    SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                }
            }
        }
        else {
            if (PickupItemDefinition->IsStackable()) {
                if (PickupItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()) || PickupItemDefinition->IsA(UFortResourceItemDefinition::StaticClass()) ||
                    PickupItemDefinition->IsA(UFortTrapItemDefinition::StaticClass())) {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                }
                else {
                    if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary) {
                        if (FortInventory::IsInventoryFull(PC)) {
                            FFortItemEntry* CurrentWeaponItemEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);

                            SpawnPickup(CurrentWeapon->WeaponData, CurrentWeaponItemEntry->Count, CurrentWeaponItemEntry->LoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                            FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                        }
                        else {
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                        }
                    }
                    else {
                        if (FortInventory::IsInventoryFull(PC)) {
                            SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                        }
                        else {
                            FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, 0);
                        }
                    }
                }
            }
            else {
                if (PickupItemDefinition->IsA(UFortTrapItemDefinition::StaticClass())) {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo);
                }
                else if (FortInventory::IsInventoryFull(PC)) {
                    if (FortInventory::GetQuickBars(CurrentWeapon->WeaponData) == EFortQuickBars::Primary) {
                        FFortItemEntry* CurrentWeaponItemEntry = FortInventory::FindItemEntry(PC, CurrentWeapon->WeaponData);

                        SpawnPickup(CurrentWeapon->WeaponData, CurrentWeaponItemEntry->Count, CurrentWeaponItemEntry->LoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                        FortInventory::RemoveItem(PC, CurrentWeapon->WeaponData);
                        FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo);
                    }
                    else {
                        SpawnPickup(PickupItemDefinition, PickupCount, PickupLoadedAmmo, Drop, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
                    }
                }
                else {
                    FortInventory::GiveItem(PC, PickupItemDefinition, PickupCount, PickupLoadedAmmo);
                }
            }
        }

        FortInventory::Update(PC);

        Pickup->K2_DestroyActor();
        return CompletePickupAnimationOG(Pickup);
    }

    void (*NetMulticast_Athena_BatchedDamageCuesOG)(AFortPlayerPawnAthena* Pawn, FAthenaBatchedDamageGameplayCues_Shared SharedData, FAthenaBatchedDamageGameplayCues_NonShared NonSharedData);
    void NetMulticast_Athena_BatchedDamageCues(AFortPlayerPawnAthena* Pawn, FAthenaBatchedDamageGameplayCues_Shared SharedData, FAthenaBatchedDamageGameplayCues_NonShared NonSharedData)
    {
        if (!Pawn || Pawn->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()))
            return;

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC) return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);

        if (Pawn->CurrentWeapon) {
            FFortItemEntry* ItemEntry = FortInventory::FindItemEntry(PC, Pawn->CurrentWeapon->ItemEntryGuid);
            if (!ItemEntry) return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);
            ItemEntry->LoadedAmmo = Pawn->CurrentWeapon->AmmoCount;
            FortInventory::Update(PC, ItemEntry);
        }

        return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);
    }

    void (*OnLandedOG)(AFortPlayerPawnAthena* Pawn, const FHitResult& Hit);
    void OnLanded(AFortPlayerPawnAthena* Pawn, const FHitResult& Hit)
    {
        if (Pawn && Pawn->Controller && Pawn->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()))
        {
            AFortAthenaAIBotController* BotController = (AFortAthenaAIBotController*)Pawn->Controller;
            if (BotController && BotController->Blackboard)
            {
                bool bHasJumped = BotController->Blackboard->GetValueAsBool(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_HasEverJumpedFromBusKey"));
                bool bHasLanded = BotController->Blackboard->GetValueAsBool(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey"));
                
                if (bHasJumped && !bHasLanded)
                {
                    BotController->Blackboard->SetValueAsBool(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey"), true);
                    BotController->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Glide_ExecutionStatus"), (int)EExecutionStatus::ExecutionSuccess);
                    BotController->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Dive_ExecutionStatus"), (int)EExecutionStatus::ExecutionSuccess);
                    Log("Bot landed from bus, enabling normal behavior!");
                }
            }
        }

        return OnLandedOG(Pawn, Hit);
    }

    void (*OnReloadOG)(AFortWeapon* a1, int RemoveCount);
    void OnReload(AFortWeapon* a1, int RemoveCount)
    {
        if (!a1) return OnReloadOG(a1, RemoveCount);

        AFortPlayerPawn* Pawn = (AFortPlayerPawn*)a1->GetOwner();
        if (!Pawn || !Pawn->Controller) return OnReloadOG(a1, RemoveCount);

        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC) return OnReloadOG(a1, RemoveCount);
        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)Pawn->PlayerState;
        if (!PlayerState || PlayerState->bIsABot) return OnReloadOG(a1, RemoveCount);

        FFortItemEntry* WeaponItemEntry = FortInventory::FindItemEntry(PC, a1->ItemEntryGuid);
        if (!WeaponItemEntry || !WeaponItemEntry->ItemDefinition) return OnReloadOG(a1, RemoveCount);

        UFortWorldItemDefinition* AmmoItemDef = a1->WeaponData ? a1->WeaponData->GetAmmoWorldItemDefinition_BP() : nullptr;
        if (!AmmoItemDef) return OnReloadOG(a1, RemoveCount);

        FFortItemEntry* AmmoItemEntry = FortInventory::FindItemEntry(PC, AmmoItemDef);
        if (AmmoItemEntry)
        {
            FortInventory::RemoveItem(PC, AmmoItemEntry->ItemDefinition, RemoveCount);
        }
        else
        {
            int MaxStackSize = WeaponItemEntry->ItemDefinition->MaxStackSize.Value;
            if (MaxStackSize > 1) FortInventory::RemoveItem(PC, WeaponItemEntry->ItemDefinition, RemoveCount);
        }

        WeaponItemEntry->LoadedAmmo = a1->AmmoCount;

        FortInventory::Update(PC, WeaponItemEntry);

        // Reset any blocking flags on the pawn's ability system
        if (Pawn->AbilitySystemComponent) {
            Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }

        return OnReloadOG(a1, RemoveCount);
    }

    void HookAll() {
        HookVTable(APlayerPawn_Athena_C::GetDefaultObj(), 0x208, ServerHandlePickup, nullptr);

        MH_CreateHook((LPVOID)(ImageBase + 0x1BA62B4), CompletePickupAnimation, (LPVOID*)&CompletePickupAnimationOG);

        HookVTable(AFortPlayerPawnAthena::GetDefaultObj(), 0x11B, NetMulticast_Athena_BatchedDamageCues, (LPVOID*)&NetMulticast_Athena_BatchedDamageCuesOG);

        HookVTable(AFortPlayerPawnAthena::GetDefaultObj(), 0xC8, OnLanded, (LPVOID*)&OnLandedOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x512FB08), OnReload, (LPVOID*)&OnReloadOG);

        Log("FortPlayerPawn Hooked!");
    }
}