#pragma once
#include "framework.h"
#include "FortInventory.h"
#include "BotsSpawner.h"

namespace FortAthenaAIBotController_SDK {
    
    struct FBotState_Internal {
        AFortAthenaAIBotController* Controller;
        AFortPlayerPawnAthena* Pawn;
        AFortPlayerStateAthena* PlayerState;
        
        bool bHasJumpedFromBus;
        bool bHasLanded;
        bool bHasThankedBusDriver;
        float JumpDelay;
        float NextThankTime;
        
        float LastMoveTime;
        FVector LastLocation;
        int StuckCounter;
        float LastStuckCheck;
    };
    
    static std::vector<FBotState_Internal*> BotStates_Internal;
    static bool bBotSystemInitialized = false;
    
    void MakeBotJumpFromBus(AFortAthenaAIBotController* Controller) {
        if (!Controller || !Controller->Pawn || !Controller->Blackboard) return;
        
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
        if (!GameState) return;
        
        // Select random destination (POI)
        FVector Destination = FVector();
        if (BuildingFoundations.Num() > 0) {
            AActor* DropZone = BuildingFoundations[UKismetMathLibrary::RandomIntegerInRange(0, BuildingFoundations.Num() - 1)];
            if (DropZone) {
                Destination = DropZone->K2_GetActorLocation();
            }
        }
        
        if (Destination.IsZero()) {
            Destination = Controller->Pawn->K2_GetActorLocation();
        }
        
        // Configure blackboard
        FName JumpOffBusName = ConvFName(L"AIEvaluator_JumpOffBus_Destination");
        Controller->Blackboard->SetValueAsVector(JumpOffBusName, Destination);
        
        // Teleport to aircraft and start skydiving
        AFortAthenaAircraft* Aircraft = GameState->GetAircraft(0);
        if (Aircraft) {
            Controller->Pawn->K2_TeleportTo(Aircraft->K2_GetActorLocation(), FRotator());
        }
        
        // BeginSkydiving exists in AFortPlayerPawnAthena
        AFortPlayerPawnAthena* PawnAthena = Cast<AFortPlayerPawnAthena>(Controller->Pawn);
        if (PawnAthena) {
            PawnAthena->BeginSkydiving(true);
            PawnAthena->SetHealth(100);
            PawnAthena->SetShield(0);
        }
        
        // Update player state
        if (Controller->PlayerState) {
            AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
            PlayerState->bInAircraft = false;
            PlayerState->bHasEverSkydivedFromBus = true;
            PlayerState->ForceNetUpdate();
        }
        
        Log("Bot jumped from bus!");
    }
    
    void MakeBotThankBus(AFortAthenaAIBotController* Controller) {
        if (!Controller) return;
        
        Controller->ThankBusDriver();
        Log("Bot thanked bus driver!");
    }
    
    void MakeBotMoveTo(AFortAthenaAIBotController* Controller, FVector TargetLocation) {
        if (!Controller || !Controller->Pawn) return;
        
        Controller->MoveToLocation(TargetLocation, 50.0f, false, true, false, true, nullptr, true);
    }
    
    void MakeBotShootAt(AFortAthenaAIBotController* Controller, AActor* Target) {
        if (!Controller || !Controller->Pawn || !Target) return;
        
        // Aim at target
        Controller->K2_SetFocalPoint(Target->K2_GetActorLocation());
        
        // Switch to ranged weapon if holding pickaxe
        AFortPlayerPawn* PlayerPawn = Cast<AFortPlayerPawn>(Controller->Pawn);
        AFortPlayerPawnAthena* PawnAthena = Cast<AFortPlayerPawnAthena>(Controller->Pawn);
        
        if (PlayerPawn && PlayerPawn->CurrentWeapon && PlayerPawn->CurrentWeapon->WeaponData) {
            if (PlayerPawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                if (Controller->Inventory) {
                    for (auto& Entry : Controller->Inventory->Inventory.ReplicatedEntries) {
                        if (Entry.ItemDefinition && Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                            if (!Entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                                // EquipWeaponDefinition exists in AFortPlayerPawnAthena
                                if (PawnAthena) {
                                    PawnAthena->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition,
                                        Entry.ItemGuid, Entry.TrackerGuid, false);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // Fire
        if (PawnAthena) {
            PawnAthena->PawnStartFire(0);
        }
    }
    
    void TickBotSystem() {
        if (!bBotSystemInitialized) return;
        
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
        if (!GameState) return;
        
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        // Process all spawned bots
        for (FortAthenaAIBotController::BotSpawnData& SpawnedBot : FortAthenaAIBotController::SpawnedBots) {
            AFortAthenaAIBotController* Controller = SpawnedBot.Controller;
            if (!Controller || !Controller->Pawn || !Controller->PlayerState) continue;
            
            AFortPlayerPawnAthena* Pawn = Cast<AFortPlayerPawnAthena>(Controller->Pawn);
            if (!Pawn) continue;
            
            AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
            
            // Get or create internal state
            FBotState_Internal* State = nullptr;
            for (auto* S : BotStates_Internal) {
                if (S && S->Controller == Controller) {
                    State = S;
                    break;
                }
            }
            
            if (!State) {
                State = new FBotState_Internal();
                State->Controller = Controller;
                State->Pawn = Pawn;
                State->PlayerState = PlayerState;
                State->bHasJumpedFromBus = false;
                State->bHasLanded = false;
                State->bHasThankedBusDriver = false;
                State->JumpDelay = CurrentTime + UKismetMathLibrary::RandomFloatInRange(3.0f, 8.0f);
                State->NextThankTime = CurrentTime + UKismetMathLibrary::RandomFloatInRange(1.0f, 4.0f);
                State->LastMoveTime = CurrentTime;
                State->LastLocation = FVector();
                State->StuckCounter = 0;
                State->LastStuckCheck = 0;
                BotStates_Internal.push_back(State);
            }
            
            // Skip dead bots
            if (Pawn->GetHealth() <= 0) continue;
            
            // Thank bus driver
            if (PlayerState->bInAircraft && !State->bHasThankedBusDriver && CurrentTime >= State->NextThankTime) {
                MakeBotThankBus(Controller);
                State->bHasThankedBusDriver = true;
            }
            
            // Jump from bus
            if (PlayerState->bInAircraft && !State->bHasJumpedFromBus && !GameState->bAircraftIsLocked && CurrentTime >= State->JumpDelay) {
                MakeBotJumpFromBus(Controller);
                State->bHasJumpedFromBus = true;
            }
            
            // Check if landed
            if (State->bHasJumpedFromBus && !State->bHasLanded) {
                if (!Pawn->IsSkydiving()) {
                    State->bHasLanded = true;
                    Log("Bot has landed!");
                }
            }
            
            // Post-landing behavior
            if (State->bHasLanded) {
                // Stuck detection
                if (CurrentTime - State->LastStuckCheck >= 2.0f) {
                    State->LastStuckCheck = CurrentTime;
                    
                    FVector CurrentLocation = Pawn->K2_GetActorLocation();
                    float DistanceMoved = SDKUtils::Dist(State->LastLocation, CurrentLocation);
                    
                    if (DistanceMoved < 50.0f && Controller->GetMoveStatus() == EPathFollowingStatus::Moving) {
                        State->StuckCounter++;
                        
                        if (State->StuckCounter >= 3) {
                            Controller->StopMovement();
                            Pawn->Jump();
                            
                            FVector RandomDest = CurrentLocation;
                            RandomDest.X += UKismetMathLibrary::RandomFloatInRange(-500.0f, 500.0f);
                            RandomDest.Y += UKismetMathLibrary::RandomFloatInRange(-500.0f, 500.0f);
                            
                            MakeBotMoveTo(Controller, RandomDest);
                            State->StuckCounter = 0;
                            
                            Log("Bot unstuck!");
                        }
                    } else {
                        State->StuckCounter = 0;
                    }
                    
                    State->LastLocation = CurrentLocation;
                }
            }
        }
    }
    
    void Initialize() {
        if (bBotSystemInitialized) return;
        
        bBotSystemInitialized = true;
        Log("BotSystem SDK Initialized!");
    }
    
    void HookAll() {
        Initialize();
        Log("BotSystem SDK Hooked!");
    }
}