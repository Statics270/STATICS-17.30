#pragma once
#include "framework.h"
#include "BehaviorTreeSystem.h"
#include "BehaviorTreeTasks.h"
#include "BehaviorTreeDecorators.h"
#include <map>

namespace BotsSpawner {
    void SpawnPlayerBot(AActor* OverrideSpawn = nullptr);
}

namespace AdvancedBotBehavior {
    // POI-based bot spawning
    struct POISpawnPoint {
        std::string POIName;
        FVector Location;
        int MaxBots;
        int CurrentBots;
    };

    static std::vector<POISpawnPoint> POISpawnPoints;
    static bool bPOISystemInitialized = false;

    void InitializePOISystem() {
        if (bPOISystemInitialized) return;

        // Find all named locations on the map that could be POIs
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AActor::StaticClass(), &AllActors);

        std::map<std::string, std::vector<FVector>> POILocations;

        for (AActor* Actor : AllActors) {
            if (!Actor) continue;

            std::string ActorName = Actor->GetName();
            FVector Location = Actor->K2_GetActorLocation();

            // Look for common POI naming patterns
            if (ActorName.find("POI") != std::string::npos || ActorName.find("Location") != std::string::npos ||
                ActorName.find("Tomb") != std::string::npos || ActorName.find("Compound") != std::string::npos ||
                ActorName.find("Base") != std::string::npos || ActorName.find("Camp") != std::string::npos) {

                std::string POIName = ActorName;

                // Extract base POI name
                size_t pos = POIName.find('_');
                if (pos != std::string::npos) {
                    POIName = POIName.substr(0, pos);
                }

                POILocations[POIName].push_back(Location);
            }
        }

        // Create spawn points from POIs
        for (auto& Entry : POILocations) {
            auto& POIName = Entry.first;
            auto& Locations = Entry.second;

            if (Locations.size() >= 2) {
                POISpawnPoint Point;
                Point.POIName = POIName;
                Point.Location = Locations[0]; // Use first location as center
                Point.MaxBots = (int)(Locations.size() / 2) < 10 ? (int)(Locations.size() / 2) : 10; // Scale bots by POI size
                Point.CurrentBots = 0;
                POISpawnPoints.push_back(Point);
            }
        }

        // Add manual POIs if none found
        if (SDKUtils::empty(POISpawnPoints)) {
            // Fallback to player start locations as POIs
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerStartWarmup::StaticClass(), &AllActors);
            int BotsPerPOI = 5;
            for (int i = 0; i < AllActors.Num() && POISpawnPoints.size() < 20; i += BotsPerPOI) {
                if (AllActors[i]) {
                    POISpawnPoint Point;
                    Point.POIName = "POI_" + std::to_string(POISpawnPoints.size());
                    Point.Location = AllActors[i]->K2_GetActorLocation();
                    Point.MaxBots = BotsPerPOI;
                    Point.CurrentBots = 0;
                    POISpawnPoints.push_back(Point);
                }
            }
        }

        bPOISystemInitialized = true;
        Log("Initialized POI System with " + std::to_string(POISpawnPoints.size()) + " POIs!");
    }

    FVector GetPOISpawnLocation() {
        if (SDKUtils::empty(POISpawnPoints)) {
            InitializePOISystem();
        }

        // Find POIs with available bot slots
        std::vector<size_t> AvailablePOIIndices;
        for (size_t i = 0; i < POISpawnPoints.size(); i++) {
            if (POISpawnPoints[i].CurrentBots < POISpawnPoints[i].MaxBots) {
                AvailablePOIIndices.push_back(i);
            }
        }

        if (SDKUtils::empty(AvailablePOIIndices)) {
            // Random fallback location
            if (!SDKUtils::empty(BuildingFoundations)) {
                return BuildingFoundations[UKismetMathLibrary::RandomIntegerInRange(0, BuildingFoundations.Num() - 1)]->K2_GetActorLocation();
            }
            return FVector();
        }

        // Select random POI
        size_t SelectedIndex = AvailablePOIIndices[UKismetMathLibrary::RandomIntegerInRange(0, AvailablePOIIndices.size() - 1)];
        POISpawnPoints[SelectedIndex].CurrentBots++;

        // Add some randomness to location within POI
        FVector SpawnLoc = POISpawnPoints[SelectedIndex].Location;
        SpawnLoc.X += UKismetMathLibrary::RandomFloatInRange(-1000, 1000);
        SpawnLoc.Y += UKismetMathLibrary::RandomFloatInRange(-1000, 1000);

        return SpawnLoc;
    }

    void SpawnBotAtPOI() {
        if (!Globals::bPOIBasedSpawning) {
            BotsSpawner::SpawnPlayerBot();
            return;
        }

        FVector SpawnLoc = GetPOISpawnLocation();
        if (SpawnLoc.IsZero()) {
            Log("Failed to get POI spawn location!");
            return;
        }

        // Use a player start at the POI location
        AFortPlayerStartWarmup* SpawnPoint = nullptr;
        for (auto* Start : PlayerStarts) {
            if (Start && SDKUtils::Dist(Start->K2_GetActorLocation(), SpawnLoc) < 5000.f) {
                SpawnPoint = (AFortPlayerStartWarmup*)Start;
                break;
            }
        }

        if (!SpawnPoint) {
            Log("Failed to find spawn point near POI!");
            return;
        }

        BotsSpawner::SpawnPlayerBot(SpawnPoint);
        Log("Spawned bot at POI!");
    }

    // Looting behavior for bots
    class BTTask_LootNearby : public BTNode {
    public:
        float LootRadius = 2000.f;
        float WaitTime = 2.f;
        float WorldWaitTime = 0.f;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotLootingEnabled) {
                return EBTNodeResult::Failed;
            }

            if (!Context.Pawn || !Context.Controller) {
                return EBTNodeResult::Failed;
            }

            float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

            // Find nearby pickups
            TArray<AActor*> NearbyPickups;
            TArray<AActor*> AllPickups;
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPickup::StaticClass(), &AllPickups);

            FVector BotLoc = Context.Pawn->K2_GetActorLocation();
            AFortPickup* BestPickup = nullptr;
            float BestDist = LootRadius;

            for (AActor* Actor : AllPickups) {
                AFortPickup* Pickup = Cast<AFortPickup>(Actor);
                if (!Pickup || Pickup->bPickedUp) continue;

                float Dist = SDKUtils::Dist(BotLoc, Pickup->K2_GetActorLocation());
                if (Dist < BestDist) {
                    BestDist = Dist;
                    BestPickup = Pickup;
                }
            }

            if (BestPickup) {
                // Move towards pickup
                if (BestDist > 150.f) {
                    Context.Controller->MoveToLocation(BestPickup->K2_GetActorLocation(), 50.f, false, true, false, true, nullptr, true);
                    return EBTNodeResult::InProgress;
                }
                else {
                    // Try to pickup
                    Context.Pawn->ServerHandlePickup(BestPickup, 0.3f, FVector(), true);
                    WorldWaitTime = CurrentTime + WaitTime;
                    return EBTNodeResult::Succeeded;
                }
            }

            return EBTNodeResult::Failed;
        }
    };

    // Combat behavior for bots
    class BTTask_EngageEnemy : public BTNode {
    public:
        float CombatRange = 5000.f;
        float ShootingRange = 1500.f;
        bool bShouldBuild = false;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotCombatEnabled) {
                return EBTNodeResult::Failed;
            }

            if (!Context.Pawn || !Context.Controller) {
                return EBTNodeResult::Failed;
            }

            AFortPlayerPawnAthena* BotPawn = Context.Pawn;
            AFortAthenaAIBotController* BotController = Context.Controller;

            // Find nearest enemy
            TArray<AActor*> AllPawns;
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerPawnAthena::StaticClass(), &AllPawns);

            AFortPlayerPawnAthena* Target = nullptr;
            float BestDist = CombatRange;
            FVector BotLoc = BotPawn->K2_GetActorLocation();

            for (AActor* Actor : AllPawns) {
                AFortPlayerPawnAthena* EnemyPawn = Cast<AFortPlayerPawnAthena>(Actor);
                if (!EnemyPawn || EnemyPawn == BotPawn) continue;
                if (!EnemyPawn->PlayerState) continue;
                if (EnemyPawn->PlayerState->bIsABot && SDKUtils::GetTeam(EnemyPawn->PlayerState) == SDKUtils::GetTeam(BotPawn->PlayerState)) continue;

                float Dist = SDKUtils::Dist(BotLoc, EnemyPawn->K2_GetActorLocation());
                if (Dist < BestDist) {
                    BestDist = Dist;
                    Target = EnemyPawn;
                }
            }

            if (Target) {
                // Aim at target
                BotController->K2_SetFocalPoint(Target->K2_GetActorLocation());

                if (BestDist <= ShootingRange && BotPawn->CurrentWeapon && BotPawn->CurrentWeapon->WeaponData) {
                    // Switch to weapon if holding pickaxe
                    if (BotPawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                        for (auto& Entry : BotController->Inventory->Inventory.ReplicatedEntries) {
                            if (Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                                BotPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, Entry.ItemGuid, Entry.TrackerGuid, false);
                                break;
                            }
                        }
                    }

                    // Enable shooting in blackboard
                    BotController->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_ManageWeapon_Fire"), true);
                }
                else {
                    // Move towards target
                    BotController->MoveToLocation(Target->K2_GetActorLocation(), 200.f, true, true, false, true, nullptr, true);
                }

                return EBTNodeResult::InProgress;
            }

            // No target found, disable shooting
            BotController->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_ManageWeapon_Fire"), false);
            return EBTNodeResult::Failed;
        }
    };

    // Building behavior for bots
    class BTTask_BuildCover : public BTNode {
    public:
        float EnemyRange = 3000.f;
        int MaxBuildAttempts = 3;
        int CurrentBuildAttempts = 0;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotBuildingEnabled) {
                return EBTNodeResult::Failed;
            }

            if (!Context.Pawn || !Context.Controller) {
                return EBTNodeResult::Failed;
            }

            if (CurrentBuildAttempts >= MaxBuildAttempts) {
                CurrentBuildAttempts = 0;
                return EBTNodeResult::Failed;
            }

            AFortPlayerPawnAthena* BotPawn = Context.Pawn;
            AFortAthenaAIBotController* BotController = Context.Controller;

            // Find nearest enemy
            TArray<AActor*> AllPawns;
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerPawnAthena::StaticClass(), &AllPawns);

            bool bEnemyNearby = false;
            FVector EnemyDir = FVector();
            FVector BotLoc = BotPawn->K2_GetActorLocation();

            for (AActor* Actor : AllPawns) {
                AFortPlayerPawnAthena* EnemyPawn = Cast<AFortPlayerPawnAthena>(Actor);
                if (!EnemyPawn || EnemyPawn == BotPawn) continue;
                if (!EnemyPawn->PlayerState || EnemyPawn->PlayerState->bIsABot) continue;

                float Dist = SDKUtils::Dist(BotLoc, EnemyPawn->K2_GetActorLocation());
                if (Dist < EnemyRange) {
                    bEnemyNearby = true;
                    EnemyDir = SDKUtils::GetSafeNormal(EnemyPawn->K2_GetActorLocation() - BotLoc);
                    break;
                }
            }

            if (bEnemyNearby) {
                // Build wall between bot and enemy
                FVector BuildLoc = BotLoc + (EnemyDir * 300.f);
                BuildLoc.Z += 100.f;

                // Equip build tool
                for (auto& Entry : BotController->Inventory->Inventory.ReplicatedEntries) {
                    if (Entry.ItemDefinition && Entry.ItemDefinition->IsA(UFortBuildingItemDefinition::StaticClass())) {
                        BotPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, Entry.ItemGuid, Entry.TrackerGuid, false);
                        break;
                    }
                }

                if (BotPawn->CurrentWeapon && BotPawn->CurrentWeapon->WeaponData->IsA(UFortBuildingItemDefinition::StaticClass())) {
                    // Try to build
                    CurrentBuildAttempts++;
                    return EBTNodeResult::Succeeded;
                }
            }

            CurrentBuildAttempts = 0;
            return EBTNodeResult::Failed;
        }
    };

    // Lobby emotes behavior
    class BTTask_PlayLobbyEmote : public BTNode {
    public:
        float EmoteInterval = 10.f;
        float LastEmoteTime = 0.f;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotEmotesEnabled) {
                return EBTNodeResult::Failed;
            }

            if (!Context.Pawn || !Context.Controller) {
                return EBTNodeResult::Failed;
            }

            AFortPlayerStateAthena* PlayerState = Context.PlayerState;
            if (!PlayerState || SDKUtils::empty(Dances)) {
                return EBTNodeResult::Failed;
            }

            AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
            if (GameState->GamePhase != EAthenaGamePhase::Warmup) {
                return EBTNodeResult::Failed;
            }

            float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

            // Check if we should play an emote
            if (CurrentTime - LastEmoteTime >= EmoteInterval) {
                LastEmoteTime = CurrentTime;

                // Check if emote is allowed
                bool bEmoteAllowed = Context.Controller->Blackboard->GetValueAsBool(
                    UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_WarmupPlayEmote_ExecutionStatus")
                );

                if (bEmoteAllowed) {
                    // Play random emote from global dances array
                    int EmoteIndex = UKismetMathLibrary::RandomIntegerInRange(0, Dances.size() - 1);
                    UAthenaDanceItemDefinition* Emote = Dances[EmoteIndex];

                    if (Emote && Context.Controller) {
                        // ServerPlayEmoteItem is on the Controller, not Pawn
                        AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Context.Controller;
                        PC->ServerPlayEmoteItem(Emote, UKismetMathLibrary::RandomFloatInRange(0.f, 1.f));
                        return EBTNodeResult::Succeeded;
                    }
                }
            }

            return EBTNodeResult::Failed;
        }
    };

    // Lobby shooting behavior (practice shots)
    class BTTask_LobbyShoot : public BTNode {
    public:
        float ShootInterval = 15.f;
        float LastShootTime = 0.f;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotLobbyShooting) {
                return EBTNodeResult::Failed;
            }

            if (!Context.Pawn || !Context.Controller) {
                return EBTNodeResult::Failed;
            }

            AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
            if (GameState->GamePhase != EAthenaGamePhase::Warmup) {
                return EBTNodeResult::Failed;
            }

            // Check if shooting is allowed
            bool bShootAllowed = Context.Controller->Blackboard->GetValueAsBool(
                UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_WarmupLootAndShoot_ExecutionStatus")
            );

            if (!bShootAllowed) {
                return EBTNodeResult::Failed;
            }

            float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

            if (CurrentTime - LastShootTime >= ShootInterval) {
                LastShootTime = CurrentTime;

                // Switch to weapon
                for (auto& Entry : Context.Controller->Inventory->Inventory.ReplicatedEntries) {
                    if (Entry.ItemDefinition && Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                        if (!Entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                            Context.Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, Entry.ItemGuid, Entry.TrackerGuid, false);
                            break;
                        }
                    }
                }

                // Fire a few shots
                if (Context.Pawn->CurrentWeapon && !Context.Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                    FVector ShootDir = Context.Pawn->GetActorForwardVector();
                    FVector AimLoc = Context.Pawn->K2_GetActorLocation() + (ShootDir * 5000.f);
                    Context.Controller->K2_SetFocalPoint(AimLoc);
                    Context.Pawn->PawnStartFire(0);
                    return EBTNodeResult::Succeeded;
                }
            }

            return EBTNodeResult::Failed;
        }
    };

    // Update the player bots behavior tree to include advanced behaviors
    void Initialize() {
        Log("Initializing Advanced Bot Behavior System...");
        InitializePOISystem();
    }
}
