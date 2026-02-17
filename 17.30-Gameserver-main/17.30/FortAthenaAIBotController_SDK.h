#pragma once
#include "framework.h"
#include "FortInventory.h"
#include "BotsSpawner.h"

namespace FortAthenaAIBotController_SDK {
    // POI locations for C2S7 map
    static std::vector<std::pair<std::string, FVector>> POILocations = {
        {"Believer_Beach", FVector(100000.0f, 200000.0f, 0.0f)},
        {"Boney_Burbs", FVector(150000.0f, 180000.0f, 0.0f)},
        {"Catty_Corner", FVector(120000.0f, 220000.0f, 0.0f)},
        {"Condo_Canyon", FVector(180000.0f, 200000.0f, 0.0f)},
        {"Dirty_Docks", FVector(200000.0f, 180000.0f, 0.0f)},
        {"Holly_Hedges", FVector(140000.0f, 160000.0f, 0.0f)},
        {"Lazy_Lake", FVector(160000.0f, 220000.0f, 0.0f)},
        {"Lofty_Links", FVector(120000.0f, 180000.0f, 0.0f)},
        {"Misty_Meadows", FVector(200000.0f, 220000.0f, 0.0f)},
        {"Pleasant_Park", FVector(80000.0f, 180000.0f, 0.0f)},
        {"Slurpy_Swamp", FVector(220000.0f, 160000.0f, 0.0f)},
        {"The_Reef", FVector(60000.0f, 200000.0f, 0.0f)},
        {"Weeping_Woods", FVector(180000.0f, 160000.0f, 0.0f)}
    };
    
    // Internal bot state tracking
    struct FBotState_Internal {
        AFortAthenaAIBotController* Controller;
        AFortPlayerPawnAthena* Pawn;
        AFortPlayerStateAthena* PlayerState;
        
        // Bus/Flight state
        bool bHasJumpedFromBus;
        bool bHasLanded;
        bool bHasThankedBusDriver;
        float JumpDelay;
        float NextThankTime;
        
        // Movement state
        float LastMoveTime;
        FVector LastLocation;
        int StuckCounter;
        float LastStuckCheck;
        
        // Combat state
        AActor* CurrentTarget;
        float LastCombatTime;
        float LastCombatScanTime;
        
        // Storm flee state
        float LastStormCheck;
        bool bIsFleeingStorm;
    };
    
    static std::vector<FBotState_Internal*> BotStates_Internal;
    static bool bBotSystemInitialized = false;
    static int SpawnedBotCount = 0;
    
    // Get or create internal bot state
    FBotState_Internal* GetOrCreateBotState(AFortAthenaAIBotController* Controller) {
        if (!Controller) return nullptr;
        
        for (auto* State : BotStates_Internal) {
            if (State && State->Controller == Controller) {
                return State;
            }
        }
        
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        FBotState_Internal* NewState = new FBotState_Internal();
        NewState->Controller = Controller;
        NewState->Pawn = (AFortPlayerPawnAthena*)Controller->Pawn;
        NewState->PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
        NewState->bHasJumpedFromBus = false;
        NewState->bHasLanded = false;
        NewState->bHasThankedBusDriver = false;
        NewState->JumpDelay = CurrentTime + UKismetMathLibrary::RandomFloatInRange(3.0f, 8.0f);
        NewState->NextThankTime = CurrentTime + UKismetMathLibrary::RandomFloatInRange(1.0f, 4.0f);
        NewState->LastMoveTime = CurrentTime;
        NewState->LastLocation = FVector();
        NewState->StuckCounter = 0;
        NewState->LastStuckCheck = 0;
        NewState->CurrentTarget = nullptr;
        NewState->LastCombatTime = 0;
        NewState->LastCombatScanTime = 0;
        NewState->LastStormCheck = 0;
        NewState->bIsFleeingStorm = false;
        
        BotStates_Internal.push_back(NewState);
        SpawnedBotCount++;
        
        Log("Created internal bot state! Total bots: " + std::to_string(SpawnedBotCount));
        return NewState;
    }
    
    // Remove bot state
    void RemoveBotState(AFortAthenaAIBotController* Controller) {
        for (size_t i = 0; i < BotStates_Internal.size(); i++) {
            if (BotStates_Internal[i] && BotStates_Internal[i]->Controller == Controller) {
                delete BotStates_Internal[i];
                BotStates_Internal.erase(BotStates_Internal.begin() + i);
                SpawnedBotCount--;
                break;
            }
        }
    }
    
    // === SPAWN FUNCTIONS ===
    
    void SpawnBotAtLocation(FVector Location, const std::string& BotName) {
        AFortGameModeAthena* GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
        if (!GameMode) return;
        
        // Find closest player start
        AActor* BestStart = nullptr;
        float BestDistance = 10000.0f;
        
        for (AActor* Start : PlayerStarts) {
            if (!Start) continue;
            
            float Distance = SDKUtils::Dist(Start->K2_GetActorLocation(), Location);
            if (Distance < BestDistance) {
                BestDistance = Distance;
                BestStart = Start;
            }
        }
        
        if (!BestStart) {
            return;
        }
        
        FortAthenaAIBotController::BotSpawnData BotSpawnData;
        
        FTransform Transform{};
        Transform.Translation = BestStart->K2_GetActorLocation();
        Transform.Rotation = FQuat();
        Transform.Scale3D = FVector{1, 1, 1};
        
        static auto PhoebeSpawnerData = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C");
        auto ComponentList = UFortAthenaAIBotSpawnerData::CreateComponentListFromClass(PhoebeSpawnerData, UWorld::GetWorld());
        
        int32 RequestID = ((UAthenaAISystem*)UWorld::GetWorld()->AISystem)->AISpawner->RequestSpawn(ComponentList, Transform);
        BotSpawnData.RequestID = RequestID;
        BotSpawnData.BotSpawnerData = PhoebeSpawnerData;
        BotSpawnData.Name = BotName;
        FortAthenaAIBotController::SpawnedBots.push_back(BotSpawnData);
        
        Log("Spawned bot at POI: " + BotName);
    }
    
    void SpawnBotsInPOIs(int BotsPerPOI = 8) {
        if (!Globals::bBotsEnabled) return;
        
        for (auto& [Name, Location] : POILocations) {
            // Add random offset
            FVector RandomOffset = FVector(
                UKismetMathLibrary::RandomFloatInRange(-2000.0f, 2000.0f),
                UKismetMathLibrary::RandomFloatInRange(-2000.0f, 2000.0f),
                UKismetMathLibrary::RandomFloatInRange(0.0f, 500.0f)
            );
            
            SpawnBotAtLocation(Location + RandomOffset, Name);
        }
        
        Log("Spawned bots in all POIs!");
    }
    
    // === BOT BEHAVIOR FUNCTIONS ===
    
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
        
        // Configure blackboard for jump
        Controller->Blackboard->SetValueAsVector(
            ConvFName(L"AIEvaluator_JumpOffBus_Destination"), Destination
        );
        Controller->Blackboard->SetValueAsVector(
            ConvFName(L"AIEvaluator_Dive_Destination"), Destination
        );
        Controller->Blackboard->SetValueAsVector(
            ConvFName(L"AIEvaluator_Glide_Destination"), Destination
        );
        
        Controller->Blackboard->SetValueAsEnum(
            ConvFName(L"AIEvaluator_Dive_ExecutionStatus"),
            (int)EExecutionStatus::ExecutionAllowed
        );
        Controller->Blackboard->SetValueAsEnum(
            ConvFName(L"AIEvaluator_Glide_ExecutionStatus"),
            (int)EExecutionStatus::ExecutionDenied
        );
        
        // Teleport to aircraft and start skydiving
        AFortAthenaAircraft* Aircraft = GameState->GetAircraft(0);
        if (Aircraft) {
            Controller->Pawn->K2_TeleportTo(Aircraft->K2_GetActorLocation(), FRotator());
        }
        Controller->Pawn->BeginSkydiving(true);
        Controller->Pawn->SetHealth(100);
        Controller->Pawn->SetShield(0);
        
        // Update player state
        if (Controller->PlayerState) {
            Controller->PlayerState->bInAircraft = false;
            Controller->PlayerState->bHasEverSkydivedFromBus = true;
            Controller->PlayerState->ForceNetUpdate();
        }
        
        // Update blackboard
        Controller->Blackboard->SetValueAsBool(
            ConvFName(L"AIEvaluator_Global_IsInBus"), false
        );
        Controller->Blackboard->SetValueAsBool(
            ConvFName(L"AIEvaluator_Global_HasEverJumpedFromBusKey"), true
        );
        
        Log("Bot jumped from bus to destination!");
    }
    
    void MakeBotThankBus(AFortAthenaAIBotController* Controller) {
        if (!Controller) return;
        
        Controller->ThankBusDriver();
        if (Controller->PlayerState) {
            Controller->PlayerState->bThankedBusDriver = true;
            Controller->PlayerState->ForceNetUpdate();
        }
        
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
        if (Controller->Pawn->CurrentWeapon && Controller->Pawn->CurrentWeapon->WeaponData) {
            if (Controller->Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                if (Controller->Inventory) {
                    for (auto& Entry : Controller->Inventory->Inventory.ReplicatedEntries) {
                        if (Entry.ItemDefinition && Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                            if (!Entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                                Controller->Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition,
                                    Entry.ItemGuid, Entry.TrackerGuid, false);
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // Fire
        Controller->Pawn->PawnStartFire(0);
    }
    
    // === MAIN TICK FUNCTION ===
    void TickBotSystem() {
        if (!bBotSystemInitialized || !Globals::bBotsEnabled) return;
        
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
        if (!GameState) return;
        
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        // Process all spawned bots
        for (FortAthenaAIBotController::BotSpawnData& SpawnedBot : FortAthenaAIBotController::SpawnedBots) {
            AFortAthenaAIBotController* Controller = SpawnedBot.Controller;
            if (!Controller || !Controller->Pawn || !Controller->PlayerState) continue;
            
            // Get or create internal state
            FBotState_Internal* State = GetOrCreateBotState(Controller);
            if (!State) continue;
            
            // Update pawn and player state references
            State->Pawn = (AFortPlayerPawnAthena*)Controller->Pawn;
            State->PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
            
            if (!State->Pawn || !State->PlayerState) continue;
            if (State->Pawn->GetHealth() <= 0) continue; // Skip dead bots
            
            // === BUS PHASE ===
            if (State->PlayerState->bInAircraft) {
                // Thank bus driver
                if (!State->bHasThankedBusDriver && CurrentTime >= State->NextThankTime) {
                    MakeBotThankBus(Controller);
                    State->bHasThankedBusDriver = true;
                }
                
                // Jump from bus
                if (!State->bHasJumpedFromBus && !GameState->bAircraftIsLocked && CurrentTime >= State->JumpDelay) {
                    MakeBotJumpFromBus(Controller);
                    State->bHasJumpedFromBus = true;
                }
            }
            
            // === POST-JUMP PHASE ===
            if (State->bHasJumpedFromBus && !State->bHasLanded) {
                if (!State->Pawn->IsSkydiving() && !State->Pawn->IsParachuteOpen()) {
                    State->bHasLanded = Controller->Blackboard ? true : true;
                    if (Controller->Blackboard) {
                        Controller->Blackboard->SetValueAsBool(
                            ConvFName(L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey"), true
                        );
                        Controller->Blackboard->SetValueAsEnum(
                            ConvFName(L"AIEvaluator_Glide_ExecutionStatus"),
                            (int)EExecutionStatus::ExecutionSuccess
                        );
                        Controller->Blackboard->SetValueAsEnum(
                            ConvFName(L"AIEvaluator_Dive_ExecutionStatus"),
                            (int)EExecutionStatus::ExecutionSuccess
                        );
                    }
                    Log("Bot has landed!");
                }
            }
            
            // === POST-LANDING PHASE ===
            if (State->bHasLanded) {
                // Stuck detection
                if (CurrentTime - State->LastStuckCheck >= 2.0f) {
                    State->LastStuckCheck = CurrentTime;
                    
                    FVector CurrentLocation = State->Pawn->K2_GetActorLocation();
                    float DistanceMoved = SDKUtils::Dist(State->LastLocation, CurrentLocation);
                    
                    if (DistanceMoved < 50.0f && Controller->GetMoveStatus() == EPathFollowingStatus::Moving) {
                        State->StuckCounter++;
                        
                        if (State->StuckCounter >= 3) {
                            // Bot is stuck - unstuck it
                            Controller->StopMovement();
                            State->Pawn->Jump();
                            
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
                
                // Storm flee
                if (Globals::bFleeStormEnabled && GameState->SafeZoneIndicator && !GameState->SafeZoneIndicator->bPaused) {
                    FVector SafeZoneCenter = GameState->SafeZoneIndicator->NextCenter;
                    float SafeZoneRadius = GameState->SafeZoneIndicator->NextRadius;
                    
                    FVector BotLocation = State->Pawn->K2_GetActorLocation();
                    float DistanceToSafeZone = SDKUtils::Dist(BotLocation, SafeZoneCenter);
                    
                    // If outside safe zone, flee
                    if (DistanceToSafeZone > SafeZoneRadius) {
                        FVector Direction = SDKUtils::GetSafeNormal(SafeZoneCenter - BotLocation);
                        FVector TargetLoc = BotLocation + (Direction * 1000.0f);
                        
                        MakeBotMoveTo(Controller, TargetLoc);
                    }
                }
                
                // Combat
                if (Globals::bBotCombatEnabled && CurrentTime - State->LastCombatScanTime >= 1.0f) {
                    State->LastCombatScanTime = CurrentTime;
                    
                    // Find nearest enemy
                    AActor* BestTarget = nullptr;
                    float BestDistance = 5000.0f;
                    FVector BotLocation = State->Pawn->K2_GetActorLocation();
                    
                    TArray<AActor*> AllPawns;
                    UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerPawnAthena::StaticClass(), &AllPawns);
                    
                    for (AActor* Actor : AllPawns) {
                        AFortPlayerPawnAthena* EnemyPawn = Cast<AFortPlayerPawnAthena>(Actor);
                        if (!EnemyPawn || EnemyPawn == State->Pawn) continue;
                        if (!EnemyPawn->PlayerState || EnemyPawn->PlayerState->bIsABot) continue;
                        if (EnemyPawn->GetHealth() <= 0) continue;
                        
                        float Distance = SDKUtils::Dist(BotLocation, EnemyPawn->K2_GetActorLocation());
                        if (Distance < BestDistance) {
                            BestDistance = Distance;
                            BestTarget = EnemyPawn;
                        }
                    }
                    
                    if (BestTarget) {
                        State->CurrentTarget = BestTarget;
                        
                        float DistanceToTarget = SDKUtils::Dist(BotLocation, BestTarget->K2_GetActorLocation());
                        
                        if (DistanceToTarget <= 1500.0f) {
                            // Shoot at target
                            MakeBotShootAt(Controller, BestTarget);
                        } else {
                            // Move closer
                            MakeBotMoveTo(Controller, BestTarget->K2_GetActorLocation());
                        }
                    }
                }
            }
        }
    }
    
    // === INITIALIZATION ===
    void Initialize() {
        if (bBotSystemInitialized) return;
        
        bBotSystemInitialized = true;
        Log("BotSystem SDK Initialized!");
    }
    
    void HookAll() {
        Initialize();
        
        // Hooks will be handled by existing systems
        
        Log("BotSystem SDK Hooked!");
    }
}
