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
    // POI-based bot spawning for C2S7
    struct POISpawnPoint {
        std::string POIName;
        FVector Location;
        int Tier; // 1-3 (petit, moyen, grand)
        int MinBots;
        int MaxBots;
        int CurrentBots;
        bool bIsStrategic; // Zones stratégiques (bosses, NPCs)
        bool bHasBoss;
        int LootQuality; // 1-5 (qualité du loot)
    };

    static std::vector<POISpawnPoint> POISpawnPoints;
    static bool bPOISystemInitialized = false;

    void InitializePOISystem() {
        if (bPOISystemInitialized) return;

        // C2S7 POI Definitions - Chapter 2 Season 7 (Invasion)
        // Approximate locations for major POIs on Apollo map
        std::vector<POISpawnPoint> C2S7_POIs = {
            // Grands POI (Tier 3) - 10-15 bots each
            {"Believer_Beach", FVector(125000, 125000, 2000), 3, 10, 15, 0, false, false, 5},
            {"Boney_Burbs", FVector(135000, 85000, 3000), 3, 10, 15, 0, false, false, 5},
            {"Catty_Corner", FVector(85000, 120000, 3500), 3, 10, 15, 0, true, true, 5}, // Avec boss
            {"Condo_Canyon", FVector(75000, 65000, 2500), 3, 10, 15, 0, false, false, 5},
            {"Dirty_Docks", FVector(180000, 150000, 1500), 3, 10, 15, 0, false, false, 5},
            {"Holly_Hedges", FVector(110000, 150000, 2200), 3, 10, 15, 0, false, false, 5},

            // POI moyens (Tier 2) - 5-10 bots each
            {"Pleasant_Park", FVector(150000, 120000, 2800), 2, 5, 10, 0, false, false, 4},
            {"Retail_Row", FVector(180000, 180000, 2500), 2, 5, 10, 0, false, false, 4},
            {"Salty_Towers", FVector(140000, 180000, 2200), 2, 5, 10, 0, false, false, 4},
            {"Lazy_Lake", FVector(90000, 160000, 2100), 2, 5, 10, 0, false, false, 4},

            // Petits POI (Tier 1) - 2-5 bots each
            {"Weeping_Woods", FVector(100000, 110000, 2400), 1, 2, 5, 0, false, false, 3},
            {"Misty_Motels", FVector(170000, 170000, 2300), 1, 2, 5, 0, false, false, 3},
            {"Steamy_Stacks", FVector(60000, 80000, 2600), 1, 2, 5, 0, false, false, 3},
            {"Coral_Castle", FVector(130000, 20000, 1800), 1, 2, 5, 0, false, false, 3},
        };

        // Try to find actual POI locations in the map
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AActor::StaticClass(), &AllActors);

        for (auto& POI : C2S7_POIs) {
            // Try to find actual location by name
            bool bFoundLocation = false;
            for (AActor* Actor : AllActors) {
                if (!Actor) continue;
                std::string ActorName = Actor->GetName();
                if (ActorName.find(POI.POIName) != std::string::npos) {
                    POI.Location = Actor->K2_GetActorLocation();
                    bFoundLocation = true;
                    break;
                }
            }

            POI.CurrentBots = 0;
            POISpawnPoints.push_back(POI);
        }

        // If no POIs found, scan for named locations
        if (SDKUtils::empty(POISpawnPoints)) {
            std::map<std::string, std::vector<FVector>> POILocations;

            for (AActor* Actor : AllActors) {
                if (!Actor) continue;
                std::string ActorName = Actor->GetName();
                FVector Location = Actor->K2_GetActorLocation();

                if (ActorName.find("POI") != std::string::npos || ActorName.find("Location") != std::string::npos ||
                    ActorName.find("Tomb") != std::string::npos || ActorName.find("Compound") != std::string::npos ||
                    ActorName.find("Base") != std::string::npos || ActorName.find("Camp") != std::string::npos) {

                    std::string POIName = ActorName;
                    size_t pos = POIName.find('_');
                    if (pos != std::string::npos) {
                        POIName = POIName.substr(0, pos);
                    }
                    POILocations[POIName].push_back(Location);
                }
            }

            for (auto& Entry : POILocations) {
                if (Entry.second.size() >= 2) {
                    POISpawnPoint Point;
                    Point.POIName = Entry.first;
                    Point.Location = Entry.second[0];
                    Point.Tier = (Entry.second.size() > 5) ? 3 : (Entry.second.size() > 2) ? 2 : 1;
                    Point.MinBots = Point.Tier * 2;
                    Point.MaxBots = Point.Tier * 5;
                    Point.CurrentBots = 0;
                    Point.bIsStrategic = false;
                    Point.bHasBoss = false;
                    Point.LootQuality = Point.Tier + 2;
                    POISpawnPoints.push_back(Point);
                }
            }
        }

        // Fallback to player starts if still no POIs
        if (SDKUtils::empty(POISpawnPoints)) {
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerStartWarmup::StaticClass(), &AllActors);
            for (int i = 0; i < AllActors.Num() && POISpawnPoints.size() < 20; i += 3) {
                if (AllActors[i]) {
                    POISpawnPoint Point;
                    Point.POIName = "SpawnZone_" + std::to_string(POISpawnPoints.size());
                    Point.Location = AllActors[i]->K2_GetActorLocation();
                    Point.Tier = 2;
                    Point.MinBots = 3;
                    Point.MaxBots = 8;
                    Point.CurrentBots = 0;
                    Point.bIsStrategic = false;
                    Point.bHasBoss = false;
                    Point.LootQuality = 3;
                    POISpawnPoints.push_back(Point);
                }
            }
        }

        bPOISystemInitialized = true;
        Log("Initialized C2S7 POI System with " + std::to_string(POISpawnPoints.size()) + " POIs!");
    }

    FVector GetPOISpawnLocation() {
        if (SDKUtils::empty(POISpawnPoints)) {
            InitializePOISystem();
        }

        // Find POIs with available bot slots, prioritizing higher tiers
        std::vector<std::pair<int, size_t>> AvailablePOIs; // (weight, index)
        for (size_t i = 0; i < POISpawnPoints.size(); i++) {
            auto& POI = POISpawnPoints[i];
            if (POI.CurrentBots < POI.MaxBots) {
                // Weight based on tier - higher tiers have higher chance
                int Weight = POI.Tier * 10;
                if (POI.bIsStrategic) Weight += 5;
                AvailablePOIs.push_back({Weight, i});
            }
        }

        if (SDKUtils::empty(AvailablePOIs)) {
            // Reset all POIs if full (new game)
            for (auto& POI : POISpawnPoints) {
                POI.CurrentBots = 0;
            }
            // Try again
            return GetPOISpawnLocation();
        }

        // Weighted random selection
        int TotalWeight = 0;
        for (auto& Entry : AvailablePOIs) {
            TotalWeight += Entry.first;
        }

        int RandomWeight = UKismetMathLibrary::RandomIntegerInRange(0, TotalWeight - 1);
        size_t SelectedIndex = 0;
        int CurrentWeight = 0;
        for (auto& Entry : AvailablePOIs) {
            CurrentWeight += Entry.first;
            if (RandomWeight < CurrentWeight) {
                SelectedIndex = Entry.second;
                break;
            }
        }

        POISpawnPoints[SelectedIndex].CurrentBots++;

        // Add randomness to location within POI based on tier
        FVector SpawnLoc = POISpawnPoints[SelectedIndex].Location;
        float Spread = POISpawnPoints[SelectedIndex].Tier * 800.f; // Higher tier = more spread
        SpawnLoc.X += UKismetMathLibrary::RandomFloatInRange(-Spread, Spread);
        SpawnLoc.Y += UKismetMathLibrary::RandomFloatInRange(-Spread, Spread);
        SpawnLoc.Z += UKismetMathLibrary::RandomFloatInRange(0, 500); // Some vertical variation

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

                // Find a nearby player to shoot at (instead of random direction)
                TArray<AActor*> AllPawns;
                UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerPawnAthena::StaticClass(), &AllPawns);

                AFortPlayerPawnAthena* TargetPlayer = nullptr;
                float BestDistance = 5000.f;

                FVector BotLoc = Context.Pawn->K2_GetActorLocation();

                for (AActor* Actor : AllPawns) {
                    AFortPlayerPawnAthena* PlayerPawn = Cast<AFortPlayerPawnAthena>(Actor);
                    if (!PlayerPawn) continue;
                    if (!PlayerPawn->PlayerState || PlayerPawn->PlayerState->bIsABot) continue;

                    float Distance = SDKUtils::Dist(BotLoc, PlayerPawn->K2_GetActorLocation());
                    if (Distance < BestDistance) {
                        BestDistance = Distance;
                        TargetPlayer = PlayerPawn;
                    }
                }

                // Switch to weapon
                for (auto& Entry : Context.Controller->Inventory->Inventory.ReplicatedEntries) {
                    if (Entry.ItemDefinition && Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                        if (!Entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                            Context.Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, Entry.ItemGuid, Entry.TrackerGuid, false);
                            break;
                        }
                    }
                }

                // Fire shots at player or in random direction
                if (Context.Pawn->CurrentWeapon && !Context.Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                    FVector AimLoc;
                    if (TargetPlayer) {
                        AimLoc = TargetPlayer->K2_GetActorLocation();
                    } else {
                        FVector ShootDir = Context.Pawn->GetActorForwardVector();
                        AimLoc = Context.Pawn->K2_GetActorLocation() + (ShootDir * 5000.f);
                    }
                    Context.Controller->K2_SetFocalPoint(AimLoc);
                    Context.Pawn->PawnStartFire(0);
                    return EBTNodeResult::Succeeded;
                }
            }

            return EBTNodeResult::Failed;
        }
    };

    // Flee storm behavior
    class BTTask_FleeStorm : public BTNode {
    public:
        float StormSafeDistance = 5000.f;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Context.Pawn || !Context.Controller) return EBTNodeResult::Failed;

            AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
            if (!GameState || !GameState->SafeZoneIndicator) return EBTNodeResult::Failed;

            FVector BotLocation = Context.Pawn->K2_GetActorLocation();
            FVector SafeZoneCenter = GameState->SafeZoneIndicator->K2_GetActorLocation();
            float SafeZoneRadius = GameState->SafeZoneIndicator->CurrentRadius;

            float DistanceToSafeZone = SDKUtils::Dist(BotLocation, SafeZoneCenter);

            // If in safe zone, don't need to flee
            if (DistanceToSafeZone <= SafeZoneRadius - 500.f) {
                return EBTNodeResult::Failed;
            }

            // If outside safe zone, flee toward center
            FVector Direction = SDKUtils::GetSafeNormal(SafeZoneCenter - BotLocation);
            FVector TargetLoc = BotLocation + (Direction * 2000.f);

            Context.Controller->MoveToLocation(TargetLoc, 100.f, true, true, false, true, nullptr, true);
            return EBTNodeResult::InProgress;
        }
    };

    // Advanced building behavior
    class BTTask_AdvancedBuild : public BTNode {
    public:
        enum EBuildType {
            Wall,
            Floor,
            Stair,
            Roof
        };

        float EnemyRange = 3000.f;
        int MaxBuildAttempts = 5;
        EBuildType BuildType;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotBuildingEnabled) return EBTNodeResult::Failed;
            if (!Context.Pawn || !Context.Controller) return EBTNodeResult::Failed;

            AFortPlayerPawnAthena* BotPawn = Context.Pawn;
            AFortAthenaAIBotController* BotController = Context.Controller;

            // Find nearest enemy
            TArray<AActor*> AllPawns;
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPlayerPawnAthena::StaticClass(), &AllPawns);

            AActor* Enemy = nullptr;
            float BestDist = EnemyRange;
            FVector BotLoc = BotPawn->K2_GetActorLocation();

            for (AActor* Actor : AllPawns) {
                AFortPlayerPawnAthena* EnemyPawn = Cast<AFortPlayerPawnAthena>(Actor);
                if (!EnemyPawn || EnemyPawn == BotPawn) continue;
                if (!EnemyPawn->PlayerState || EnemyPawn->PlayerState->bIsABot) continue;

                float Dist = SDKUtils::Dist(BotLoc, EnemyPawn->K2_GetActorLocation());
                if (Dist < BestDist) {
                    BestDist = Dist;
                    Enemy = EnemyPawn;
                }
            }

            if (!Enemy) return EBTNodeResult::Failed;

            FVector EnemyLocation = Enemy->K2_GetActorLocation();
            float DistanceToEnemy = SDKUtils::Dist(BotLoc, EnemyLocation);

            // Determine build type based on situation
            FVector BuildLoc = FVector();

            if (DistanceToEnemy < 500.f) {
                // Enemy very close - build wall for protection
                BuildType = Wall;
                FVector Direction = SDKUtils::GetSafeNormal(EnemyLocation - BotLoc);
                BuildLoc = BotLoc + (Direction * 200.f);
                BuildLoc.Z += 100.f;
            } else if (DistanceToEnemy < 1500.f) {
                // Medium range - build stairs for height advantage
                BuildType = Stair;
                FVector Direction = SDKUtils::GetSafeNormal(EnemyLocation - BotLoc);
                BuildLoc = BotLoc + (Direction * 100.f);
            } else {
                // Far away - build defensive wall
                BuildType = Wall;
                BuildLoc = BotLoc;
            }

            // Equip build tool
            if (BotController->Inventory) {
                for (auto& Entry : BotController->Inventory->Inventory.ReplicatedEntries) {
                    if (Entry.ItemDefinition && Entry.ItemDefinition->IsA(UFortBuildingItemDefinition::StaticClass())) {
                        BotPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition,
                            Entry.ItemGuid, Entry.TrackerGuid, false);
                        break;
                    }
                }
            }

            if (BotPawn->CurrentWeapon && BotPawn->CurrentWeapon->WeaponData->IsA(UFortBuildingItemDefinition::StaticClass())) {
                // Set build type and location
                BotController->Blackboard->SetValueAsVector(
                    UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Building_BuildLocation"),
                    BuildLoc
                );
                return EBTNodeResult::Succeeded;
            }

            return EBTNodeResult::Failed;
        }
    };

    // Smart looting behavior
    class BTTask_SmartLoot : public BTNode {
    public:
        float LootRadius = 3000.f;

        EBTNodeResult ChildTask(BTContext Context) override {
            if (!Globals::bBotLootingEnabled) return EBTNodeResult::Failed;
            if (!Context.Pawn || !Context.Controller) return EBTNodeResult::Failed;

            AFortPlayerPawnAthena* BotPawn = Context.Pawn;
            AFortAthenaAIBotController* BotController = Context.Controller;

            float Health = BotPawn->GetHealth();
            float Shield = BotPawn->GetShield();

            // Priority system: Shield low > Need weapon > Other loot
            AFortPickup* BestPickup = nullptr;
            float BestScore = -1.f;
            float BestDist = LootRadius;
            FVector BotLoc = BotPawn->K2_GetActorLocation();

            TArray<AActor*> AllPickups;
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPickup::StaticClass(), &AllPickups);

            for (AActor* Actor : AllPickups) {
                AFortPickup* Pickup = Cast<AFortPickup>(Actor);
                if (!Pickup || Pickup->bPickedUp) continue;

                float Dist = SDKUtils::Dist(BotLoc, Pickup->K2_GetActorLocation());
                if (Dist > BestDist) continue;

                float Score = 0;
                auto ItemDef = Pickup->PrimaryPickupItemEntry.ItemDefinition;

                if (ItemDef) {
                    // Shield is highest priority when low
                    if (Shield < 50.f && ItemDef->IsA(UFortConsumableItemDefinition::StaticClass())) {
                        Score += 1000.f;
                    }
                    // Weapons are second priority
                    else if (!HasGoodWeapon(BotPawn) && ItemDef->ItemType == EFortItemType::Weapon) {
                        Score += 500.f;
                    }
                    // Other items have lower priority
                    else {
                        Score += 100.f;
                    }

                    // Score based on distance (closer is better)
                    Score += (LootRadius - Dist) / 10.f;

                    if (Score > BestScore) {
                        BestScore = Score;
                        BestPickup = Pickup;
                        BestDist = Dist;
                    }
                }
            }

            if (BestPickup) {
                if (BestDist > 150.f) {
                    // Move towards pickup
                    Context.Controller->MoveToLocation(BestPickup->K2_GetActorLocation(), 50.f, false, true, false, true, nullptr, true);
                    return EBTNodeResult::InProgress;
                } else {
                    // Pickup the item
                    Context.Pawn->ServerHandlePickup(BestPickup, 0.3f, FVector(), true);
                    return EBTNodeResult::Succeeded;
                }
            }

            return EBTNodeResult::Failed;
        }

    private:
        bool HasGoodWeapon(AFortPlayerPawnAthena* Pawn) {
            if (!Pawn || !Pawn->CurrentWeapon || !Pawn->CurrentWeapon->WeaponData) return false;

            // Check if currently holding a ranged weapon (not pickaxe)
            if (Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                return false;
            }

            // Check if weapon has ammo
            if (Pawn->CurrentWeapon->AmmoCount <= 0) {
                return false;
            }

            return true;
        }
    };

    // Update the player bots behavior tree to include advanced behaviors
    void Initialize() {
        Log("Initializing Advanced Bot Behavior System...");
        InitializePOISystem();
    }
}
