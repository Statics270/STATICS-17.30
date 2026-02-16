#pragma once
#include "framework.h"
#include "QuestSystem.h"

namespace BotFixes {
    // Bot state tracking for improved AI behavior
    struct FBotState {
        AFortAthenaAIBotController* Controller;
        AFortPlayerPawnAthena* Pawn;
        AFortPlayerStateAthena* PlayerState;
        
        // Movement state
        float LastMoveTime;
        FVector LastLocation;
        int StuckCounter;
        float LastStuckCheck;
        
        // Combat state
        float LastCombatTime;
        float LastCombatScanTime;
        AActor* CurrentTarget;
        float TargetSwitchCooldown;
        
        // Action state
        float LastActionTime;
        bool bCanUseConsumables;
        bool bCanReload;
        bool bCanHarvest;
        bool bCanInteract;
        
        // Bus/Flight state
        bool bHasJumpedFromBus;
        bool bHasLanded;
        bool bHasThankedBusDriver;
        float JumpDelay;
        float NextThankTime;

        // Interaction throttles
        float LastInteractionScanTime;
    };

    static std::vector<FBotState*> BotStates;
    static bool bBotFixesInitialized = false;

    FBotState* GetOrCreateBotState(AFortAthenaAIBotController* Controller) {
        if (!Controller) return nullptr;

        for (auto* State : BotStates) {
            if (State && State->Controller == Controller) {
                return State;
            }
        }

        FBotState* NewState = new FBotState();
        NewState->Controller = Controller;
        NewState->Pawn = (AFortPlayerPawnAthena*)Controller->Pawn;
        NewState->PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        NewState->LastMoveTime = CurrentTime;
        NewState->LastLocation = FVector();
        NewState->StuckCounter = 0;
        NewState->LastStuckCheck = 0;
        NewState->LastCombatTime = 0;
        NewState->LastCombatScanTime = 0;
        NewState->CurrentTarget = nullptr;
        NewState->TargetSwitchCooldown = 0;
        NewState->LastActionTime = 0;
        NewState->bCanUseConsumables = true;
        NewState->bCanReload = true;
        NewState->bCanHarvest = true;
        NewState->bCanInteract = true;
        NewState->bHasJumpedFromBus = false;
        NewState->bHasLanded = false;
        NewState->bHasThankedBusDriver = false;
        NewState->JumpDelay = CurrentTime + UKismetMathLibrary::RandomFloatInRange(3.0f, 8.0f);
        NewState->NextThankTime = CurrentTime + UKismetMathLibrary::RandomFloatInRange(1.0f, 4.0f);
        NewState->LastInteractionScanTime = 0;

        BotStates.push_back(NewState);
        return NewState;
    }

    void RemoveBotState(AFortAthenaAIBotController* Controller) {
        for (size_t i = 0; i < BotStates.size(); i++) {
            if (BotStates[i] && BotStates[i]->Controller == Controller) {
                delete BotStates[i];
                BotStates.erase(BotStates.begin() + i);
                break;
            }
        }
    }

    // Fix 1: Bot Movement - Check if bot is stuck and handle it
    void UpdateBotMovement(FBotState* State) {
        if (!State || !State->Pawn || !State->Controller || !Globals::bBotStuckDetection) return;

        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        FVector CurrentLocation = State->Pawn->K2_GetActorLocation();

        // Check if bot is stuck every 2 seconds
        if (CurrentTime - State->LastStuckCheck >= 2.0f) {
            State->LastStuckCheck = CurrentTime;

            float DistanceMoved = SDKUtils::Dist(State->LastLocation, CurrentLocation);
            
            // If bot hasn't moved much, it might be stuck
            if (DistanceMoved < 50.0f && State->Controller->GetMoveStatus() == EPathFollowingStatus::Moving) {
                State->StuckCounter++;
                
                if (State->StuckCounter >= 3) {
                    // Bot is stuck, try to unstuck
                    State->Controller->StopMovement();
                    
                    // Try to jump
                    State->Pawn->Jump();
                    
                    // Pick a random direction to move
                    FVector RandomDest = CurrentLocation;
                    RandomDest.X += UKismetMathLibrary::RandomFloatInRange(-500.0f, 500.0f);
                    RandomDest.Y += UKismetMathLibrary::RandomFloatInRange(-500.0f, 500.0f);
                    
                    State->Controller->MoveToLocation(RandomDest, 50.0f, false, true, false, true, nullptr, true);
                    State->StuckCounter = 0;
                    
                    Log("Bot unstuck attempt triggered!");
                }
            } else {
                State->StuckCounter = 0;
            }

            State->LastLocation = CurrentLocation;
        }
    }

    // Fix 2: Bot Bus Jump - Ensure bots properly exit the bus
    void UpdateBotBusBehavior(FBotState* State) {
        if (!State || !State->Pawn || !State->Controller || !State->PlayerState || !State->Controller->Blackboard) return;

        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
        if (!GameState) return;

        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        if (!Globals::bBotBusJumpFix) {
            return;
        }

        if (State->PlayerState->bThankedBusDriver) {
            State->bHasThankedBusDriver = true;
        }

        if (State->PlayerState->bInAircraft && !State->bHasThankedBusDriver) {
            if (CurrentTime >= State->NextThankTime) {
                State->Controller->ThankBusDriver();
                State->PlayerState->bThankedBusDriver = true;
                State->PlayerState->ForceNetUpdate();
                State->bHasThankedBusDriver = true;
            }
        }

        // Check if bot should jump from bus
        if (State->PlayerState->bInAircraft && !State->bHasJumpedFromBus) {
            // Check if aircraft is unlocked
            if (!GameState->bAircraftIsLocked) {
                // Check if we've waited long enough
                if (CurrentTime >= State->JumpDelay) {
                    FVector Destination = State->Controller->Blackboard->GetValueAsVector(
                        UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_JumpOffBus_Destination")
                    );

                    if (Destination.IsZero() && BuildingFoundations.Num() > 0) {
                        AActor* DropZone = BuildingFoundations[UKismetMathLibrary::RandomIntegerInRange(0, BuildingFoundations.Num() - 1)];
                        if (DropZone) {
                            Destination = DropZone->K2_GetActorLocation();
                        }
                    }

                    if (Destination.IsZero()) {
                        Destination = State->Pawn->K2_GetActorLocation();
                    }

                    // Perform bus jump
                    State->Controller->Blackboard->SetValueAsVector(
                        ConvFName(L"AIEvaluator_Dive_Destination"), Destination
                    );
                    State->Controller->Blackboard->SetValueAsVector(
                        ConvFName(L"AIEvaluator_Glide_Destination"), Destination
                    );

                    State->Controller->Blackboard->SetValueAsEnum(
                        ConvFName(L"AIEvaluator_Dive_ExecutionStatus"), 
                        (int)EExecutionStatus::ExecutionAllowed
                    );
                    State->Controller->Blackboard->SetValueAsEnum(
                        ConvFName(L"AIEvaluator_Glide_ExecutionStatus"), 
                        (int)EExecutionStatus::ExecutionDenied
                    );

                    // Teleport to aircraft and start skydiving
                    AFortAthenaAircraft* Aircraft = GameState->GetAircraft(0);
                    if (Aircraft) {
                        State->Pawn->K2_TeleportTo(Aircraft->K2_GetActorLocation(), FRotator());
                    }
                    State->Pawn->BeginSkydiving(true);
                    State->Pawn->SetHealth(100);
                    State->Pawn->SetShield(0);

                    State->PlayerState->bInAircraft = false;
                    State->PlayerState->bHasEverSkydivedFromBus = true;
                    State->bHasJumpedFromBus = true;

                    State->Controller->Blackboard->SetValueAsBool(
                        ConvFName(L"AIEvaluator_Global_IsInBus"), false
                    );
                    State->Controller->Blackboard->SetValueAsBool(
                        ConvFName(L"AIEvaluator_Global_HasEverJumpedFromBusKey"), true
                    );

                    Log("Bot successfully jumped from bus!");
                }
            }
        }

        // Handle landing
        if (State->bHasJumpedFromBus && !State->bHasLanded) {
            if (!State->Pawn->IsSkydiving() && !State->Pawn->IsParachuteOpen()) {
                // Bot has landed
                State->bHasLanded = true;
                State->Controller->Blackboard->SetValueAsBool(
                    ConvFName(L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey"), true
                );
                State->Controller->Blackboard->SetValueAsEnum(
                    ConvFName(L"AIEvaluator_Glide_ExecutionStatus"), 
                    (int)EExecutionStatus::ExecutionSuccess
                );
                State->Controller->Blackboard->SetValueAsEnum(
                    ConvFName(L"AIEvaluator_Dive_ExecutionStatus"), 
                    (int)EExecutionStatus::ExecutionSuccess
                );
                
                Log("Bot has landed!");
            }
        }
    }

    // Fix 3: Bot Damage - Ensure bots can take damage
    void FixBotDamage(AFortPlayerPawnAthena* Pawn) {
        if (!Pawn || !Globals::bBotDamageFix) return;

        // Ensure bot is not invulnerable
        Pawn->bIsInvulnerable = false;
        Pawn->bCanBeDamaged = true;
        
        // Ensure bot has health
        if (Pawn->GetHealth() <= 0) {
            Pawn->SetHealth(100);
        }

        // Clear any blocking gameplay effects
        if (Pawn->AbilitySystemComponent) {
            Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
        }
    }

    // Fix 4: Bot Actions - Enable consumables, reload, harvesting
    void UpdateBotActions(FBotState* State) {
        if (!State || !State->Pawn || !State->Controller) return;

        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        // Only process actions every second
        if (CurrentTime - State->LastActionTime < 1.0f) return;
        State->LastActionTime = CurrentTime;

        if (!State->Controller->Inventory) {
            return;
        }

        // Check health and use consumables if needed
        float Health = State->Pawn->GetHealth();
        float Shield = State->Pawn->GetShield();
        
        if (State->bCanUseConsumables && Health < 50.0f) {
            // Try to find and use healing item
            for (auto& Entry : State->Controller->Inventory->Inventory.ReplicatedEntries) {
                if (!Entry.ItemDefinition) continue;
                
                // Check if it's a consumable that heals
                if (Entry.ItemDefinition->IsA(UFortConsumableItemDefinition::StaticClass())) {
                    auto Consumable = (UFortConsumableItemDefinition*)Entry.ItemDefinition;
                    // Use the consumable
                    State->Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, 
                        Entry.ItemGuid, Entry.TrackerGuid, false);
                    break;
                }
            }
        }

        // Check ammo and reload if needed
        if (State->bCanReload && State->Pawn->CurrentWeapon) {
            if (State->Pawn->CurrentWeapon->AmmoCount <= 0) {
                // Try to reload
                State->Controller->Blackboard->SetValueAsBool(
                    ConvFName(L"AIEvaluator_ManageWeapon_Reload"), true
                );
            }
        }
    }

    // Fix 5: Bot Combat - Improved targeting and shooting
    void UpdateBotCombat(FBotState* State) {
        if (!State || !State->Pawn || !State->Controller || !State->Controller->Blackboard || !Globals::bBotCombatEnabled) return;

        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        if (CurrentTime - State->LastCombatScanTime < 1.0f) {
            return;
        }
        State->LastCombatScanTime = CurrentTime;

        // Find nearest enemy
        TArray<AActor*> AllPawns;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerPawnAthena::StaticClass(), &AllPawns);

        AActor* BestTarget = nullptr;
        float BestScore = -1.0f;
        FVector BotLocation = State->Pawn->K2_GetActorLocation();

        for (AActor* Actor : AllPawns) {
            AFortPlayerPawnAthena* EnemyPawn = Cast<AFortPlayerPawnAthena>(Actor);
            if (!EnemyPawn || EnemyPawn == State->Pawn) continue;
            if (!EnemyPawn->PlayerState) continue;
            if (EnemyPawn->PlayerState->bIsABot) continue; // Don't target other bots for now
            if (EnemyPawn->GetHealth() <= 0) continue; // Don't target dead players

            FVector EnemyLocation = EnemyPawn->K2_GetActorLocation();
            float Distance = SDKUtils::Dist(BotLocation, EnemyLocation);

            // Don't target if too far
            if (Distance > 5000.0f) continue;

            // Calculate score based on distance (closer is better)
            float Score = 5000.0f - Distance;

            if (Score > BestScore) {
                BestScore = Score;
                BestTarget = EnemyPawn;
            }
        }

        // Update target
        if (BestTarget && CurrentTime >= State->TargetSwitchCooldown) {
            State->CurrentTarget = BestTarget;
            State->LastCombatTime = CurrentTime;
            State->TargetSwitchCooldown = CurrentTime + 2.0f;

            // Engage target
            FVector TargetLocation = BestTarget->K2_GetActorLocation();
            State->Controller->K2_SetFocalPoint(TargetLocation);

            // Switch to weapon if holding pickaxe
            if (State->Pawn->CurrentWeapon && State->Pawn->CurrentWeapon->WeaponData) {
                if (State->Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                    // Find a ranged weapon
                    if (State->Controller->Inventory) {
                        for (auto& Entry : State->Controller->Inventory->Inventory.ReplicatedEntries) {
                            if (Entry.ItemDefinition && Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                                if (!Entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                                    State->Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition,
                                        Entry.ItemGuid, Entry.TrackerGuid, false);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Enable firing
            float DistanceToTarget = SDKUtils::Dist(BotLocation, TargetLocation);
            if (DistanceToTarget <= 1500.0f) {
                State->Controller->Blackboard->SetValueAsBool(
                    ConvFName(L"AIEvaluator_ManageWeapon_Fire"), true
                );
                State->Pawn->PawnStartFire(0);
            } else {
                // Move closer
                State->Controller->MoveToLocation(TargetLocation, 200.0f, true, true, false, true, nullptr, true);
            }
        } else if (!BestTarget) {
            // No target, stop firing
            State->Controller->Blackboard->SetValueAsBool(
                ConvFName(L"AIEvaluator_ManageWeapon_Fire"), false
            );
            State->Pawn->PawnStopFire(0);
            State->CurrentTarget = nullptr;
        }
    }

    // Fix 6: Bot Interaction - Enable chest opening and pickup
    void UpdateBotInteraction(FBotState* State) {
        if (!State || !State->Pawn || !State->Controller || !Globals::bBotLootingEnabled) return;

        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        if (CurrentTime - State->LastInteractionScanTime < 1.5f) {
            return;
        }
        State->LastInteractionScanTime = CurrentTime;

        FVector BotLocation = State->Pawn->K2_GetActorLocation();

        // Look for nearby chests
        TArray<AActor*> AllContainers;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), ABuildingContainer::StaticClass(), &AllContainers);

        for (AActor* Actor : AllContainers) {
            ABuildingContainer* Container = Cast<ABuildingContainer>(Actor);
            if (!Container) continue;
            if (Container->bAlreadySearched) continue;

            float Distance = SDKUtils::Dist(BotLocation, Container->K2_GetActorLocation());
            
            if (Distance <= 200.0f) {
                // Open the container
                Container->bAlreadySearched = true;
                Container->OnRep_bAlreadySearched();
                
                // Notify quest system
                auto* PC = Cast<AFortPlayerControllerAthena>(State->Controller);
                if (PC) {
                    QuestSystem::OnChestOpened(PC);
                }
                break;
            } else if (Distance <= 1000.0f) {
                // Move towards container
                State->Controller->MoveToLocation(Container->K2_GetActorLocation(), 100.0f, false, true, false, true, nullptr, true);
                break;
            }
        }

        // Look for nearby pickups
        TArray<AActor*> AllPickups;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPickup::StaticClass(), &AllPickups);

        for (AActor* Actor : AllPickups) {
            AFortPickup* Pickup = Cast<AFortPickup>(Actor);
            if (!Pickup || Pickup->bPickedUp) continue;

            float Distance = SDKUtils::Dist(BotLocation, Pickup->K2_GetActorLocation());
            
            if (Distance <= 150.0f) {
                // Pickup the item
                State->Pawn->ServerHandlePickup(Pickup, 0.3f, FVector(), true);
            } else if (Distance <= 800.0f) {
                // Move towards valuable pickups only
                if (Pickup->PrimaryPickupItemEntry.ItemDefinition) {
                    auto ItemDef = Pickup->PrimaryPickupItemEntry.ItemDefinition;
                    // Prioritize weapons and healing items
                    if (ItemDef->ItemType == EFortItemType::Weapon ||
                        ItemDef->IsA(UFortConsumableItemDefinition::StaticClass())) {
                        State->Controller->MoveToLocation(Pickup->K2_GetActorLocation(), 100.0f, false, true, false, true, nullptr, true);
                        break;
                    }
                }
            }
        }
    }

    // Main tick function for all bot fixes
    void TickBotFixes() {
        if (!Globals::bBotsEnabled || !Globals::bBotFixesEnabled) return;

        for (auto* State : BotStates) {
            if (!State || !State->Controller) continue;

            // Get current pawn and player state (might change during gameplay)
            State->Pawn = (AFortPlayerPawnAthena*)State->Controller->Pawn;
            State->PlayerState = (AFortPlayerStateAthena*)State->Controller->PlayerState;

            if (!State->Pawn || !State->PlayerState) continue;
            if (State->Pawn->GetHealth() <= 0) continue; // Skip dead bots

            // Apply fixes
            FixBotDamage(State->Pawn);
            UpdateBotBusBehavior(State);
            
            // Only apply these fixes if bot has landed
            if (State->bHasLanded) {
                UpdateBotMovement(State);
                UpdateBotActions(State);
                UpdateBotCombat(State);
                UpdateBotInteraction(State);
            }
        }
    }

    // Hook for OnPossessedPawnDied to cleanup state
    void OnBotDied(AFortAthenaAIBotController* Controller) {
        if (!Controller) return;
        RemoveBotState(Controller);
    }

    // Hook for new bot spawn
    void OnBotSpawned(AFortAthenaAIBotController* Controller) {
        if (!Controller) return;
        GetOrCreateBotState(Controller);
        Log("Bot state created for new bot!");
    }

    void Initialize() {
        if (bBotFixesInitialized) return;
        
        Log("Initializing Bot Fixes System...");
        bBotFixesInitialized = true;
        
        // Clear any existing states
        for (auto* State : BotStates) {
            if (State) delete State;
        }
        BotStates.clear();
        
        Log("Bot Fixes System Initialized!");
    }

    void HookAll() {
        Initialize();
        Log("BotFixes Hooked!");
    }
}
