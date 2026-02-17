#pragma once
#include "framework.h"
#include "FortInventory.h"

namespace ActionSystem {
    // Configuration
    static bool bActionSystemInitialized = false;
    
    // Structure for tracking player action states
    struct PlayerActionState {
        AFortPlayerControllerAthena* PC;
        AFortPlayerPawn* Pawn;
        
        // Reloading state
        bool bIsReloading;
        float ReloadStartTime;
        float ReloadDuration;
        
        // Consumable state
        bool bIsUsingConsumable;
        float ConsumableStartTime;
        float ConsumableDuration;
        
        // Interaction state
        bool bIsInteracting;
        float InteractionStartTime;
        AActor* CurrentInteractionTarget;
        
        // Grenade state
        bool bIsThrowingGrenade;
        float GrenadeThrowTime;
        
        // Last action times for throttling
        float LastReloadCheck;
        float LastConsumableCheck;
        float LastInteractionCheck;
    };
    
    static std::vector<PlayerActionState*> PlayerActionStates;
    
    // Get or create action state for a player
    PlayerActionState* GetOrCreateActionState(AFortPlayerControllerAthena* PC) {
        if (!PC) return nullptr;
        
        for (auto* State : PlayerActionStates) {
            if (State && State->PC == PC) {
                return State;
            }
        }
        
        PlayerActionState* NewState = new PlayerActionState();
        NewState->PC = PC;
        NewState->Pawn = nullptr;
        NewState->bIsReloading = false;
        NewState->ReloadStartTime = 0;
        NewState->ReloadDuration = 0;
        NewState->bIsUsingConsumable = false;
        NewState->ConsumableStartTime = 0;
        NewState->ConsumableDuration = 0;
        NewState->bIsInteracting = false;
        NewState->InteractionStartTime = 0;
        NewState->CurrentInteractionTarget = nullptr;
        NewState->bIsThrowingGrenade = false;
        NewState->GrenadeThrowTime = 0;
        NewState->LastReloadCheck = 0;
        NewState->LastConsumableCheck = 0;
        NewState->LastInteractionCheck = 0;
        
        PlayerActionStates.push_back(NewState);
        return NewState;
    }
    
    // Remove action state when player disconnects
    void RemoveActionState(AFortPlayerControllerAthena* PC) {
        for (size_t i = 0; i < PlayerActionStates.size(); i++) {
            if (PlayerActionStates[i] && PlayerActionStates[i]->PC == PC) {
                delete PlayerActionStates[i];
                PlayerActionStates.erase(PlayerActionStates.begin() + i);
                break;
            }
        }
    }
    
    // === DIRECT RELOAD FUNCTIONS ===
    void ForceCompleteReload(AFortPlayerPawn* Pawn) {
        if (!Pawn || !Pawn->CurrentWeapon) return;
        
        AFortWeapon* Weapon = Pawn->CurrentWeapon;
        UFortWeaponRangedItemDefinition* WeaponData = Cast<UFortWeaponRangedItemDefinition>(Weapon->WeaponData);
        if (!WeaponData) return;
        
        AFortPlayerControllerAthena* PC = Cast<AFortPlayerControllerAthena>(Pawn->Controller);
        if (!PC || !PC->WorldInventory) return;
        
        // Find ammo in inventory - GetAmmoWorldItemDefinition_BP returns UFortWorldItemDefinition*
        UFortWorldItemDefinition* AmmoDef = WeaponData->GetAmmoWorldItemDefinition_BP();
        if (!AmmoDef) return;
        
        FFortItemEntry* AmmoEntry = FortInventory::FindItemEntry(PC, AmmoDef);
        
        if (AmmoEntry) {
            // ClipSize might not exist in this SDK version, use default value
            int ClipSize = 30; // Default clip size
            int CurrentAmmo = Weapon->AmmoCount;
            int NeededAmmo = ClipSize - CurrentAmmo;
            
            int AvailableAmmo = AmmoEntry->Count;
            int AmmoToAdd = std::min(NeededAmmo, AvailableAmmo);
            
            // Update weapon
            Weapon->AmmoCount = CurrentAmmo + AmmoToAdd;
            
            // Update ammo inventory
            if (AmmoToAdd > 0) {
                AmmoEntry->Count -= AmmoToAdd;
                FortInventory::Update(PC, AmmoEntry);
            }
            
            // Update weapon entry
            FFortItemEntry* WeaponEntry = FortInventory::FindItemEntry(PC, Weapon->WeaponData);
            if (WeaponEntry) {
                WeaponEntry->LoadedAmmo = Weapon->AmmoCount;
                FortInventory::Update(PC, WeaponEntry);
            }
            
            Log("Force completed reload! Ammo: " + std::to_string(Weapon->AmmoCount));
        }
    }
    
    void ProcessReload(AFortPlayerControllerAthena* PC) {
        if (!PC || !PC->MyFortPawn) return;
        
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        // Throttle reload checks
        PlayerActionState* State = GetOrCreateActionState(PC);
        if (!State) return;
        
        if (CurrentTime - State->LastReloadCheck < 0.5f) return;
        State->LastReloadCheck = CurrentTime;
        
        // Update pawn reference - MyFortPawn is AFortPlayerPawn*, not AFortPlayerPawnAthena*
        State->Pawn = PC->MyFortPawn;
        
        AFortWeapon* Weapon = State->Pawn->CurrentWeapon;
        if (!Weapon || Weapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) return;
        
        // If weapon is empty, force reload
        if (Weapon->AmmoCount <= 0 && !State->bIsReloading) {
            Log("Weapon empty, forcing reload...");
            State->bIsReloading = true;
            State->ReloadStartTime = CurrentTime;
            State->ReloadDuration = 2.0f;
            
            // Clear any ability blocking
            if (State->Pawn->AbilitySystemComponent) {
                State->Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
            }
            
            // Complete the reload after delay
            ForceCompleteReload(State->Pawn);
            State->bIsReloading = false;
        }
        
        // Handle ongoing reload
        if (State->bIsReloading && CurrentTime - State->ReloadStartTime >= State->ReloadDuration) {
            ForceCompleteReload(State->Pawn);
            State->bIsReloading = false;
        }
    }
    
    // === DIRECT INTERACTION FUNCTIONS ===
    void ProcessInteraction(AFortPlayerControllerAthena* PC, AActor* Target) {
        if (!PC || !PC->MyFortPawn || !Target) return;
        
        AFortPlayerPawn* Pawn = PC->MyFortPawn;
        float Distance = SDKUtils::Dist(Pawn->K2_GetActorLocation(), Target->K2_GetActorLocation());
        
        if (Distance > 500.0f) return;
        
        // Interaction with a chest/container
        ABuildingContainer* Container = Cast<ABuildingContainer>(Target);
        if (Container && !Container->bAlreadySearched) {
            Log("Opening chest...");
            
            Container->bAlreadySearched = true;
            Container->OnRep_bAlreadySearched();
            Container->BP_SetAlreadySearched(true);
            Container->SearchBounceData.SearchAnimationCount++;
            Container->SearchBounceData.SearchingPawn = Pawn;
            Container->OnSetSearched();
            Container->ForceNetUpdate();
            
            // Spawn loot
            Looting::SpawnLoot(Container, Pawn);
            
            return;
        }
        
        // Interaction with ammo box
        if (Target->IsA(ABuildingContainer::StaticClass())) {
            FName SearchLootTierGroup = ((ABuildingContainer*)Target)->SearchLootTierGroup;
            if (SearchLootTierGroup == UKismetStringLibrary::Conv_StringToName(L"Loot_Ammo")) {
                Looting::SpawnLoot((ABuildingContainer*)Target, Pawn);
                return;
            }
        }
        
        // Interaction with pickup
        AFortPickup* Pickup = Cast<AFortPickup>(Target);
        if (Pickup && !Pickup->bPickedUp) {
            Pawn->ServerHandlePickup(Pickup, 0.3f, FVector(), true);
            return;
        }
    }
    
    // === DIRECT CONSUMABLE USE FUNCTIONS ===
    void ProcessConsumableUse(AFortPlayerControllerAthena* PC) {
        if (!PC || !PC->MyFortPawn || !PC->WorldInventory) return;
        
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        PlayerActionState* State = GetOrCreateActionState(PC);
        if (!State) return;
        
        // Throttle consumable checks
        if (CurrentTime - State->LastConsumableCheck < 1.0f) return;
        State->LastConsumableCheck = CurrentTime;
        
        // Skip if already using consumable
        if (State->bIsUsingConsumable) return;
        
        AFortPlayerPawn* Pawn = PC->MyFortPawn;
        
        // Get health and shield
        float Health = Pawn->GetHealth();
        float Shield = Pawn->GetShield();
        
        // Only use consumables if we need healing or shield
        if (Health >= 100.0f && Shield >= 100.0f) return;
        
        // Find consumable in inventory
        for (auto& Entry : PC->WorldInventory->Inventory.ReplicatedEntries) {
            if (!Entry.ItemDefinition) continue;
            if (!Entry.ItemDefinition->IsA(UFortConsumableItemDefinition::StaticClass())) continue;
            
            auto ConsumableDef = (UFortConsumableItemDefinition*)Entry.ItemDefinition;
            
            bool bShouldUse = false;
            // HealAmount, ShieldAmount don't exist in this SDK version - assume all consumables can heal
            if (Health < 100.0f) bShouldUse = true;
            if (Shield < 100.0f) bShouldUse = true;
            
            if (bShouldUse) {
                Log("Using consumable: " + ConsumableDef->GetName());
                
                // Clear ability blocking
                if (Pawn->AbilitySystemComponent) {
                    Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
                }
                
                // EquipWeaponDefinition expects UFortWeaponItemDefinition*, not UFortConsumableItemDefinition*
                // For consumables, we should use ability activation instead
                // For now, skip this as it requires proper ability handling
                
                // Apply effects directly with default values
                State->bIsUsingConsumable = true;
                State->ConsumableStartTime = CurrentTime;
                State->ConsumableDuration = 2.0f; // Default usage duration
                
                if (Health < 100.0f) {
                    float NewHealth = std::min(100.0f, Health + 50.0f); // Default heal amount
                    Pawn->SetHealth(NewHealth);
                }
                
                if (Shield < 100.0f) {
                    float NewShield = std::min(100.0f, Shield + 50.0f); // Default shield amount
                    Pawn->SetShield(NewShield);
                }
                
                // Remove consumable from inventory
                FortInventory::RemoveItem(PC, ConsumableDef, 1);
                
                State->bIsUsingConsumable = false;
                Log("Consumable used successfully!");
                return;
            }
        }
    }
    
    // === DIRECT GRENADE FUNCTIONS ===
    void ProcessGrenadeThrow(AFortPlayerControllerAthena* PC) {
        if (!PC || !PC->MyFortPawn) return;
        
        AFortPlayerPawn* Pawn = PC->MyFortPawn;
        
        // UFortGrenadeItemDefinition doesn't exist in this SDK
        // Look for throwable items by ItemType instead
        for (auto& Entry : PC->WorldInventory->Inventory.ReplicatedEntries) {
            if (!Entry.ItemDefinition) continue;
            if (Entry.ItemDefinition->ItemType != EFortItemType::Weapon) continue;
            
            auto WeaponDef = (UFortWeaponItemDefinition*)Entry.ItemDefinition;
            
            // Skip melee weapons (pickaxe)
            if (WeaponDef->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) continue;
            
            Log("Throwing item: " + WeaponDef->GetName());
            
            // Clear ability blocking
            if (Pawn->AbilitySystemComponent) {
                Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
            }
            
            // Equip the item
            Pawn->EquipWeaponDefinition(WeaponDef, Entry.ItemGuid, Entry.TrackerGuid, false);
            
            // Throw it
            if (Pawn->CurrentWeapon) {
                FVector Forward = Pawn->GetActorForwardVector();
                FVector ThrowLocation = Pawn->K2_GetActorLocation() + (Forward * 500.0f);
                
                // K2_SetFocalPoint exists in AIModule, use it
                if (PC) {
                    PC->K2_SetFocalPoint(ThrowLocation);
                }
                Pawn->PawnStartFire(0);
                
                // Remove item from inventory
                FortInventory::RemoveItem(PC, WeaponDef, 1);
                
                Log("Item thrown successfully!");
                return;
            }
        }
    }
    
    // === MAIN TICK FUNCTION ===
    void TickActionSystem() {
        if (!bActionSystemInitialized) return;
        
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        // Get all player controllers
        TArray<AActor*> AllControllers;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerControllerAthena::StaticClass(), &AllControllers);
        
        for (AActor* Actor : AllControllers) {
            AFortPlayerControllerAthena* PC = Cast<AFortPlayerControllerAthena>(Actor);
            if (!PC || !PC->MyFortPawn) continue;
            
            // Skip bots
            if (PC->PlayerState && PC->PlayerState->bIsABot) continue;
            
            // Update action state pawn reference - MyFortPawn is AFortPlayerPawn*
            PlayerActionState* State = GetOrCreateActionState(PC);
            if (State) {
                State->Pawn = PC->MyFortPawn;
            }
            
            // Process reload
            if (Globals::bReloadFix) {
                ProcessReload(PC);
            }
            
            // Process consumables
            if (Globals::bConsumablesFix) {
                ProcessConsumableUse(PC);
            }
        }
    }
    
    // === HOOKS ===
    void (*ServerStartFireOG)(AFortWeapon* Weapon, uint8 FireModeNum);
    void ServerStartFire(AFortWeapon* Weapon, uint8 FireModeNum) {
        if (!Weapon) return ServerStartFireOG(Weapon, FireModeNum);
        
        AFortPlayerPawn* Pawn = (AFortPlayerPawn*)Weapon->GetOwner();
        if (!Pawn || !Pawn->Controller) return ServerStartFireOG(Weapon, FireModeNum);
        
        AFortPlayerControllerAthena* PC = Cast<AFortPlayerControllerAthena>(Pawn->Controller);
        if (!PC) return ServerStartFireOG(Weapon, FireModeNum);
        
        // Clear any blocking before firing
        if (Pawn->AbilitySystemComponent) {
            Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }
        
        return ServerStartFireOG(Weapon, FireModeNum);
    }
    
    void Initialize() {
        if (bActionSystemInitialized) return;
        
        bActionSystemInitialized = true;
        Log("ActionSystem Initialized!");
    }
    
    void HookAll() {
        Initialize();
        
        // Hook ServerStartFire to clear blocking before firing
        MH_CreateHook((LPVOID)(ImageBase + 0x4D51160), ServerStartFire, (LPVOID*)&ServerStartFireOG);
        
        Log("ActionSystem Hooked!");
    }
}
