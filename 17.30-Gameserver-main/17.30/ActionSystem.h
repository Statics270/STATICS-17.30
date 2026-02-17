#pragma once
#include "framework.h"

namespace ActionSystem {
    static bool bActionSystemInitialized = false;
    
    // === FONCTION DE RECHARGEMENT SIMPLIFIÉE ===
    void ForceCompleteReload(AFortPlayerPawnAthena* Pawn) {
        if (!Pawn || !Pawn->CurrentWeapon) return;
        
        AFortWeapon* Weapon = Pawn->CurrentWeapon;
        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
        if (!PC || !PC->WorldInventory) return;
        
        // Trouver la munition dans l'inventaire
        UFortWeaponRangedItemDefinition* WeaponData = Cast<UFortWeaponRangedItemDefinition>(Weapon->WeaponData);
        if (!WeaponData) return;
        
        UFortWorldItemDefinition* AmmoDef = WeaponData->GetAmmoWorldItemDefinition_BP();
        if (!AmmoDef) return;
        
        FFortItemEntry* AmmoEntry = FortInventory::FindItemEntry(PC, AmmoDef);
        
        if (AmmoEntry && AmmoEntry->Count > 0) {
            // Calculer la quantité de munitions à ajouter
            int ClipSize = 30; // Valeur par défaut
            int CurrentAmmo = Weapon->AmmoCount;
            int NeededAmmo = ClipSize - CurrentAmmo;
            int AmmoToAdd = std::min(NeededAmmo, AmmoEntry->Count);
            
            if (AmmoToAdd > 0) {
                // Mettre à jour l'arme
                Weapon->AmmoCount = CurrentAmmo + AmmoToAdd;
                
                // Mettre à jour l'inventaire de munitions
                AmmoEntry->Count -= AmmoToAdd;
                FortInventory::Update(PC, AmmoEntry);
                
                // Mettre à jour l'entrée de l'arme
                FFortItemEntry* WeaponEntry = FortInventory::FindItemEntry(PC, Weapon->WeaponData);
                if (WeaponEntry) {
                    WeaponEntry->LoadedAmmo = Weapon->AmmoCount;
                    FortInventory::Update(PC, WeaponEntry);
                }
                
                Log("Force completed reload! Ammo: " + std::to_string(Weapon->AmmoCount));
            }
        }
    }
    
    void Initialize() {
        if (bActionSystemInitialized) return;
        bActionSystemInitialized = true;
        Log("ActionSystem Initialized!");
    }
    
    void HookAll() {
        Initialize();
        Log("ActionSystem Hooked!");
    }
}