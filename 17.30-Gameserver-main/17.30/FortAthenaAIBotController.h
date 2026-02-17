#pragma once
#include "framework.h"
#include "Looting.h"
#include "AbilitySystemComponent.h"
#include "NpcAI.h"
#include "PlayerBots.h"
#include "QuestSystem.h"

#include "PhoebeDisplayNames.h"

namespace FortAthenaAIBotController {
    // Bot state tracking for improved AI behavior (integrated from BotFixes)
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

    struct BotSpawnData {
        UClass* BotSpawnerData;
        int32 RequestID;
        FString BotIDSuffix;
        std::string Name;

        AFortAthenaAIBotController* Controller;
        AFortPlayerPawnAthena* Pawn;
        AFortPlayerStateAthena* PlayerState;
    };
    std::vector<BotSpawnData> SpawnedBots;
    
    // Bot fixes state management
    static std::vector<FBotState*> BotStates;
    static bool bBotFixesInitialized = false;

    FBotState* GetOrCreateBotState(AFortAthenaAIBotController* Controller);
    void RemoveBotState(AFortAthenaAIBotController* Controller);
    void UpdateBotMovement(FBotState* State);
    void UpdateBotBusBehavior(FBotState* State);
    void FixBotDamage(AFortPlayerPawnAthena* Pawn);
    void UpdateBotActions(FBotState* State);
    void UpdateBotCombat(FBotState* State);
    void UpdateBotInteraction(FBotState* State);
    void TickBotFixes();

    void (*CreateAndConfigureNavigationSystemOG)(UAthenaNavSystemConfig* ModuleConfig, UWorld* World);
    void CreateAndConfigureNavigationSystem(UAthenaNavSystemConfig* ModuleConfig, UWorld* World)
    {
        Log("CreateAndConfigureNavigationSystem For World: " + World->GetName() + " For NavConfig: " + ModuleConfig->GetName());
        ModuleConfig->bPrioritizeNavigationAroundSpawners = true;
        ModuleConfig->bAutoSpawnMissingNavData = true;
        ModuleConfig->bSpawnNavDataInNavBoundsLevel = true;
        return CreateAndConfigureNavigationSystemOG(ModuleConfig, World);
    }

    // Pathfinding
    void (*InitializeForWorldOG)(UNavigationSystemV1* NavSystem, UWorld* World, EFNavigationSystemRunMode Mode);
    void InitializeForWorld(UAthenaNavSystem* NavSystem, UWorld* World, EFNavigationSystemRunMode Mode)
    {
        Log("InitialiseForWorld: " + World->GetName() + " For NavSystem: " + NavSystem->GetName());
        NavSystem->bAutoCreateNavigationData = true;
        AthenaNavSystem = NavSystem;
        return InitializeForWorldOG(NavSystem, World, Mode);
    }

    void (*OnPawnAISpawnedOG)(AActor* Controller, AFortPlayerPawnAthena* Pawn);
    void OnPawnAISpawned(AActor* Controller, AFortPlayerPawnAthena* Pawn)
    {
        static int AmountTimesCalled = 0;

        AFortGameModeAthena* GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

        OnPawnAISpawnedOG(Controller, Pawn);
        if (!AthenaNavSystem->MainNavData) {
            Log("NavData Dont Exist!");
        }

        std::string BotName = "";

        UClass* BotSpawnerData = nullptr;
        auto PC = (AFortAthenaAIBotController*)Pawn->Controller;
        auto PlayerState = (AFortPlayerStateAthena*)Pawn->PlayerState;
        for (BotSpawnData& SpawnedBot : SpawnedBots) {
            if (AmountTimesCalled == SpawnedBot.RequestID) {
                SpawnedBot.BotIDSuffix = PC->BotIDSuffix;
                SpawnedBot.Controller = PC;
                SpawnedBot.Pawn = Pawn;
                SpawnedBot.PlayerState = PlayerState;
                BotName = SpawnedBot.Name;
                if (SpawnedBot.BotSpawnerData) {
                    BotSpawnerData = SpawnedBot.BotSpawnerData;
                }
            }
        }
        if (PC->BotIDSuffix.ToString().contains("Clone")) {
            Log("SloneClone");
            BotSpawnerData = StaticLoadObject<UClass>("/Slone/NPCs/Slone/SloneClone/BP_AIBotSpawnerData_SloneClone.BP_AIBotSpawnerData_SloneClone_C");
        }
        if (BotSpawnerData) {
            UFortAthenaAIBotSpawnerData* SpawnerData = Cast<UFortAthenaAIBotSpawnerData>(BotSpawnerData->DefaultObject);
            if (!SpawnerData) {
                Log("No SpawnerData!");
                return;
            }

            UFortAthenaAISpawnerDataComponent_AIBotInventory* InventoryComponent = (UFortAthenaAISpawnerDataComponent_AIBotInventory*)SpawnerData->GetInventoryComponent();
            if (!PC->StartupInventory) {
                PC->StartupInventory = (UFortAthenaAIBotInventoryItems*)UGameplayStatics::GetDefaultObj()->SpawnObject(UFortAthenaAIBotInventoryItems::StaticClass(), GameMode);
            }
            if (InventoryComponent) {
                PC->StartupInventory->Items = InventoryComponent->Items;
            }

            UFortAthenaAISpawnerDataComponent_SkillsetBase* SkillSetBase = SpawnerData->GetSkillSetComponent();
            if (SkillSetBase) {
                PC->BotSkillSetClasses = SkillSetBase->GetSkillSets();
            }

            UFortAthenaAISpawnerDataComponent_ConversationBase* ConversationComp = SpawnerData->GetConversationComponent();

            AbilitySystemComponent::GiveAbilitySet(StaticLoadObject<UFortAbilitySet>("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer"), PlayerState);
        }

        if (!PC->PathFollowingComponent->MyNavData) {
            PC->PathFollowingComponent->MyNavData = AthenaNavSystem->MainNavData;
        }
        PC->PathFollowingComponent->OnNavDataRegistered(PC->PathFollowingComponent->MyNavData);
        PC->PathFollowingComponent->Activate(true);

        if (Pawn->Controller->Class == ABP_PhoebePlayerController_C::StaticClass())
        {
            PC->Blackboard->SetValueAsEnum(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Global_GamePhaseStep"), (int)GameState->GamePhaseStep);
            PC->Blackboard->SetValueAsEnum(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Global_GamePhase"), (int)GameState->GamePhase);
            PC->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Global_HasEverJumpedFromBusKey"), false);
            PC->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Global_HasEverJumpedFromBusAndLandedKey"), false);
            PC->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Global_IsMovementBlocked"), false);

            if (UKismetMathLibrary::RandomBool()) {
                PC->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_WarmupPlayEmote_ExecutionStatus"), (int)EExecutionStatus::ExecutionAllowed);
            }
            else {
                if (UKismetMathLibrary::RandomBool()) {
                    PC->Blackboard->SetValueAsBool(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_WarmupLootAndShoot_ExecutionStatus"), (int)EExecutionStatus::ExecutionAllowed);
                }
            }

            if (BuildingFoundations.Num() > 0) {
                AActor* DropZone = BuildingFoundations[UKismetMathLibrary::RandomIntegerInRange(0, BuildingFoundations.Num() - 1)];
                if (DropZone) {
                    PC->Blackboard->SetValueAsVector(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_JumpOffBus_Destination"), DropZone->K2_GetActorLocation());
                }

                AActor* MarkerLoc = BuildingFoundations[UKismetMathLibrary::RandomIntegerInRange(0, BuildingFoundations.Num() - 1)];
                if (MarkerLoc) {
                    PC->Blackboard->SetValueAsVector(UKismetStringLibrary::Conv_StringToName(L"AIEvaluator_Marker_MarkerLocation"), MarkerLoc->K2_GetActorLocation());
                }
            }
            else {
                Log("No building foundations!");
            }

            if (!Characters.empty()) {
                auto CID = Characters[UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(0, Characters.size() - 1)];
                if (CID->HeroDefinition)
                {
                    if (CID->HeroDefinition->Specializations.IsValid())
                    {
                        for (size_t i = 0; i < CID->HeroDefinition->Specializations.Num(); i++)
                        {
                            UFortHeroSpecialization* Spec = StaticLoadObject<UFortHeroSpecialization>(UKismetStringLibrary::GetDefaultObj()->Conv_NameToString(CID->HeroDefinition->Specializations[i].ObjectID.AssetPathName).ToString());
                            if (Spec)
                            {
                                for (size_t j = 0; j < Spec->CharacterParts.Num(); j++)
                                {
                                    UCustomCharacterPart* Part = StaticLoadObject<UCustomCharacterPart>(UKismetStringLibrary::GetDefaultObj()->Conv_NameToString(Spec->CharacterParts[j].ObjectID.AssetPathName).ToString());
                                    if (Part)
                                    {
                                        PlayerState->CharacterData.Parts[(uintptr_t)Part->CharacterPartType] = Part;
                                    }
                                }
                            }
                        }
                    }
                }
                if (CID) {
                    Pawn->CosmeticLoadout.Character = CID;
                }
            }
            if (!Backpacks.empty() && UKismetMathLibrary::GetDefaultObj()->RandomBoolWithWeight(0.5)) { // less likely to equip than skin cause lots of ppl prefer not to use backpack
                auto Backpack = Backpacks[UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(0, Backpacks.size() - 1)];
                for (size_t j = 0; j < Backpack->CharacterParts.Num(); j++)
                {
                    UCustomCharacterPart* Part = Backpack->CharacterParts[j];
                    if (Part)
                    {
                        PlayerState->CharacterData.Parts[(uintptr_t)Part->CharacterPartType] = Part;
                    }
                }
            }
            if (!Gliders.empty()) {
                auto Glider = Gliders[UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(0, Gliders.size() - 1)];
                Pawn->CosmeticLoadout.Glider = Glider;
            }
            if (!Contrails.empty() && UKismetMathLibrary::GetDefaultObj()->RandomBoolWithWeight(0.95)) {
                auto Contrail = Contrails[UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(0, Contrails.size() - 1)];
                Pawn->CosmeticLoadout.SkyDiveContrail = Contrail;
            }
            for (size_t i = 0; i < Dances.size(); i++)
            {
                Pawn->CosmeticLoadout.Dances.Add(Dances.at(i));
            }
            PlayerState->OnRep_CharacterData();

            if (PhoebeDisplayNames.size() != 0) {
                std::srand(static_cast<unsigned int>(std::time(0)));
                int randomIndex = std::rand() % PhoebeDisplayNames.size();
                std::string rdName = PhoebeDisplayNames[randomIndex];
                PhoebeDisplayNames.erase(PhoebeDisplayNames.begin() + randomIndex);

                int size_needed = MultiByteToWideChar(CP_UTF8, 0, rdName.c_str(), (int)rdName.size(), NULL, 0);
                std::wstring wideString(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, rdName.c_str(), (int)rdName.size(), &wideString[0], size_needed);


                FString CVName = FString(wideString.c_str());
                GameMode->ChangeName(PC, CVName, true);

                PlayerState->OnRep_PlayerName();
            }

            for (auto& Items : ((AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode)->StartingItems)
            {
                if (!Items.Item)
                    continue;
                UFortWorldItem* Item = Cast<UFortWorldItem>(Items.Item->CreateTemporaryItemInstanceBP(Items.Count, 0));
                Item->OwnerInventory = PC->Inventory;
                FFortItemEntry& Entry = Item->ItemEntry;
                PC->Inventory->Inventory.ReplicatedEntries.Add(Entry);
                PC->Inventory->Inventory.ItemInstances.Add(Item);
                PC->Inventory->Inventory.MarkItemDirty(Entry);
                PC->Inventory->HandleInventoryLocalUpdate();
            }
        }
        else {
            ApplyCharacterCustomization(PlayerState, Pawn);
        }

        for (auto SkillSet : PC->BotSkillSetClasses)
        {
            if (!SkillSet)
                continue;

            if (auto AimingSkill = Cast<UFortAthenaAIBotAimingDigestedSkillSet>(SkillSet))
                PC->CacheAimingDigestedSkillSet = AimingSkill;

            if (auto AttackingSkill = Cast<UFortAthenaAIBotAttackingDigestedSkillSet>(SkillSet))
                PC->CacheAttackingSkillSet = AttackingSkill;

            if (auto HarvestSkill = Cast<UFortAthenaAIBotHarvestDigestedSkillSet>(SkillSet))
                PC->CacheHarvestDigestedSkillSet = HarvestSkill;

            if (auto InventorySkill = Cast<UFortAthenaAIBotInventoryDigestedSkillSet>(SkillSet))
                PC->CacheInventoryDigestedSkillSet = InventorySkill;

            if (auto LootingSkill = Cast<UFortAthenaAIBotLootingDigestedSkillSet>(SkillSet))
                PC->CacheLootingSkillSet = LootingSkill;

            if (auto MovementSkill = Cast<UFortAthenaAIBotMovementDigestedSkillSet>(SkillSet))
                PC->CacheMovementSkillSet = MovementSkill;

            if (auto PerceptionSkill = Cast<UFortAthenaAIBotPerceptionDigestedSkillSet>(SkillSet))
                PC->CachePerceptionDigestedSkillSet = PerceptionSkill;

            if (auto PlayStyleSkill = Cast<UFortAthenaAIBotPlayStyleDigestedSkillSet>(SkillSet))
                PC->CachePlayStyleSkillSet = PlayStyleSkill;

            if (auto RangeAttackSkill = Cast<UFortAthenaAIBotRangeAttackDigestedSkillSet>(SkillSet))
                PC->CacheRangeAttackSkillSet = RangeAttackSkill;

            if (auto UnstuckSkill = Cast<UFortAthenaAIBotUnstuckDigestedSkillSet>(SkillSet))
                PC->CacheUnstuckSkillSet = UnstuckSkill;
        }

        if (Globals::bBotsShouldUseManualTicking) {
            PC->BrainComponent->StopLogic(L"Manual Ticking Enabled!");
        }
        PC->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_GamePhaseStep"), (int)GameState->GamePhaseStep);
        PC->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_GamePhase"), (int)GameState->GamePhase);
        PC->Blackboard->SetValueAsBool(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_IsMovementBlocked"), false);
        PC->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_RangeAttack_ExecutionStatus"), (int)EExecutionStatus::ExecutionAllowed);

        if (Pawn->Controller->Class == ABP_PhoebePlayerController_C::StaticClass()) {
            for (auto& Items : ((AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode)->StartingItems)
            {
                if (!Items.Item)
                    continue;
                UFortWorldItem* Item = Cast<UFortWorldItem>(Items.Item->CreateTemporaryItemInstanceBP(Items.Count, 0));
                Item->OwnerInventory = PC->Inventory;
                FFortItemEntry& Entry = Item->ItemEntry;
                PC->Inventory->Inventory.ReplicatedEntries.Add(Entry);
                PC->Inventory->Inventory.ItemInstances.Add(Item);
                PC->Inventory->Inventory.MarkItemDirty(Entry);
                PC->Inventory->HandleInventoryLocalUpdate();
            }
        }

        if (PC->StartupInventory) {
            for (auto& Items : PC->StartupInventory->Items)
            {
                UFortItemDefinition* ItemDef = Items.Item;
                if (!ItemDef) {
                    continue;
                }

                UFortWorldItem* Item = (UFortWorldItem*)ItemDef->CreateTemporaryItemInstanceBP(Items.Count, 0);
                Item->OwnerInventory = PC->Inventory;
                Item->ItemEntry.LoadedAmmo = 60;
                PC->Inventory->Inventory.ReplicatedEntries.Add(Item->ItemEntry);
                PC->Inventory->Inventory.ItemInstances.Add(Item);
                PC->Inventory->Inventory.MarkItemDirty(Item->ItemEntry);
                PC->Inventory->HandleInventoryLocalUpdate();
                if (Pawn->Controller->Class == ABP_PhoebePlayerController_C::StaticClass()) {
                    if (auto WeaponDef = Cast<UFortWeaponMeleeItemDefinition>(Item->ItemEntry.ItemDefinition))
                    {
                        PC->PendingEquipWeapon = Item;
                        Pawn->EquipWeaponDefinition(WeaponDef, Item->ItemEntry.ItemGuid, Item->ItemEntry.TrackerGuid, false);
                    }
                }
                else {
                    if (auto WeaponDef = Cast<UFortWeaponRangedItemDefinition>(Item->ItemEntry.ItemDefinition))
                    {
                        PC->PendingEquipWeapon = Item;
                        Pawn->EquipWeaponDefinition(WeaponDef, Item->ItemEntry.ItemGuid, Item->ItemEntry.TrackerGuid, false);
                    }
                }
            }
        }
        else {
            Log("StartupInventory is nullptr!");
        }

        if (Pawn->Controller->Class != ABP_PhoebePlayerController_C::StaticClass()) {
            bool bSetupPatrollingComp = false;
            if (!bSetupPatrollingComp) {
                for (AFortAthenaPatrolPathPointProvider* PatrolPointProvider : GetAllActorsOfClass<AFortAthenaPatrolPathPointProvider>()) {
                    if (PatrolPointProvider->AssociatedPatrolPath && (PatrolPointProvider->Name.ToString().contains(BotName.empty() ? PC->BotIDSuffix.ToString() : BotName) || PatrolPointProvider->AssociatedPatrolPath->GetFullName().contains(BotName.empty() ? PC->BotIDSuffix.ToString() : BotName))) {
                        Log("Found Patrol Path For Name: " + (BotName.empty() ? PC->BotIDSuffix.ToString() : BotName));
                        PC->CachedPatrollingComponent->SetPatrolPath(PatrolPointProvider->AssociatedPatrolPath);
                        bSetupPatrollingComp = true;
                        break;
                    }
                }

                if (!bSetupPatrollingComp) {
                    for (AFortAthenaPatrolPath* PatrolPath : GetAllActorsOfClass<AFortAthenaPatrolPath>()) {
                        if (PatrolPath && PatrolPath->Name.ToString().contains(BotName.empty() ? PC->BotIDSuffix.ToString() : BotName)) {
                            Log("Found Patrol Path For Name: " + (BotName.empty() ? PC->BotIDSuffix.ToString() : BotName));
                            PC->CachedPatrollingComponent->SetPatrolPath(PatrolPath);
                            bSetupPatrollingComp = true;
                            break;
                        }
                    }
                }
            }
        }

        if (Pawn->Controller->Class != ABP_PhoebePlayerController_C::StaticClass()) {
            NpcAI::NpcBot* Bot = new NpcAI::NpcBot(PC, Pawn, PlayerState);
            Bot->BT_NPC = NpcAI::ConstructBehaviorTree();
        }
        else {
            PlayerBots::PhoebeBot* Bot = new PlayerBots::PhoebeBot(PC, Pawn, PlayerState);
            Bot->BT_Phoebe = PlayerBots::ConstructBehaviorTree();
        }

        // Initialize bot state for bot fixes
        GetOrCreateBotState(PC);

        AmountTimesCalled++;
    }

    void (*InventoryBaseOnSpawnedOG)(UFortAthenaAISpawnerDataComponent_InventoryBase* a1, APawn* a2);
    void InventoryBaseOnSpawned(UFortAthenaAISpawnerDataComponent_InventoryBase* a1, APawn* Pawn)
    {
        if (!Pawn || !Pawn->Controller)
            return;
        auto PC = (AFortAthenaAIBotController*)Pawn->Controller;

        if (!PC->Inventory)
            PC->Inventory = SpawnActor<AFortInventory>({}, {}, PC);

        InventoryBaseOnSpawnedOG(a1, Pawn);
    }

    void (*OnPossessedPawnDiedOG)(AFortAthenaAIBotController* PC, AActor* DamagedActor, float Damage, AController* InstigatedBy, AActor* DamageCauser, FVector HitLocation, UPrimitiveComponent* HitComp, FName BoneName, FVector Momentum);
    void OnPossessedPawnDied(AFortAthenaAIBotController* PC, AActor* DamagedActor, float Damage, AController* InstigatedBy, AActor* DamageCauser, FVector HitLocation, UPrimitiveComponent* HitComp, FName BoneName, FVector Momentum)
    {
        if (!PC || !PC->Pawn || !PC->PlayerState || !InstigatedBy) {
            return;
        }

        AFortGameModeAthena* GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

        AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)PC->Pawn;
        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)PC->PlayerState;

        AFortPlayerStateAthena* KillerState = (AFortPlayerStateAthena*)InstigatedBy->PlayerState;

        UClass* BotSpawnerData = nullptr;
        for (BotSpawnData& SpawnedBot : SpawnedBots) {
            if (SpawnedBot.Controller == PC) {
                if (SpawnedBot.BotSpawnerData) {
                    BotSpawnerData = SpawnedBot.BotSpawnerData;
                }
            }
        }
        if (BotSpawnerData) {
            UFortAthenaAIBotSpawnerData* SpawnerData = Cast<UFortAthenaAIBotSpawnerData>(BotSpawnerData->DefaultObject);
            if (!SpawnerData) {
                return;
            }

            UFortAthenaAISpawnerDataComponent_AIBotInventory* InventoryComponent = (UFortAthenaAISpawnerDataComponent_AIBotInventory*)SpawnerData->GetInventoryComponent();
            if (InventoryComponent->ShouldDropCurrencyOnDeath.Value) {
                static auto Bars = StaticLoadObject<UFortItemDefinition>("/Game/Items/ResourcePickups/Athena_WadsItemData.Athena_WadsItemData");

                SpawnPickup(Bars, UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(10, 150), 0, PC->Pawn->K2_GetActorLocation(), EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::BotElimination);
            }
        }

        if (PC->Inventory) {
            for (int32 i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
            {
                if (PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                    continue;
                }
                if (!((UFortWorldItemDefinition*)PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition)->bCanBeDropped) {
                    continue;
                }
                FFortItemEntry ItemEntry = PC->Inventory->Inventory.ReplicatedEntries[i];
                if (ItemEntry.Count <= 0) continue;
                auto Def = ItemEntry.ItemDefinition;
                SpawnPickup(Def, ItemEntry.Count, ItemEntry.LoadedAmmo, PC->Pawn->K2_GetActorLocation(), EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::BotElimination);
                UFortAmmoItemDefinition* AmmoDef = (UFortAmmoItemDefinition*)((UFortWeaponRangedItemDefinition*)Def)->GetAmmoWorldItemDefinition_BP();
                if (AmmoDef) {
                    SpawnPickup(AmmoDef, AmmoDef->DropCount, 0, PC->Pawn->K2_GetActorLocation(), EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::BotElimination);
                }
            }
        }

        // Cleanup bot state
        RemoveBotState(PC);

        // Track elimination for quest system
        if (InstigatedBy && InstigatedBy->IsA(AFortPlayerControllerAthena::StaticClass())) {
            auto* KillerPC = (AFortPlayerControllerAthena*)InstigatedBy;
            QuestSystem::OnPlayerElimination(KillerPC, (AFortPlayerControllerAthena*)PC, true);
        }

        if (Pawn->Controller->Class == ABP_PhoebePlayerController_C::StaticClass()) {
            for (int i = 0; i < PlayerBots::PhoebeBots.size(); i++) {
                if (PlayerBots::PhoebeBots[i]->PC == PC) {
                    PlayerBots::PhoebeBots[i]->bTickEnabled = false;
                }
            }

            if (!KillerState->bIsABot)
            {
                for (size_t i = 0; i < KillerState->PlayerTeam->TeamMembers.Num(); i++)
                {
                    ((AFortPlayerStateAthena*)KillerState->PlayerTeam->TeamMembers[i]->PlayerState)->TeamKillScore++;
                    ((AFortPlayerStateAthena*)KillerState->PlayerTeam->TeamMembers[i]->PlayerState)->OnRep_TeamKillScore();
                }

                KillerState->ClientReportKill(PlayerState);
                KillerState->ClientReportTeamKill(KillerState->KillScore);
                KillerState->OnRep_Kills();
            }

            FDeathInfo& DeathInfo = PlayerState->DeathInfo;
            DeathInfo.bDBNO = Pawn->bWasDBNOOnDeath;
            DeathInfo.DeathLocation = Pawn->K2_GetActorLocation();
            DeathInfo.DeathTags = Pawn->DeathTags;
            DeathInfo.Downer = KillerState ? KillerState : nullptr;
            AFortPawn* KillerPawn = KillerState ? KillerState->GetCurrentPawn() : nullptr;
            DeathInfo.Distance = (KillerPawn && Pawn) ? KillerPawn->GetDistanceTo(Pawn) : 0.f;
            DeathInfo.FinisherOrDowner = KillerState ? KillerState : nullptr;
            DeathInfo.DeathCause = PlayerState->ToDeathCause(DeathInfo.DeathTags, DeathInfo.bDBNO);
            DeathInfo.bInitialized = true;
            PlayerState->OnRep_DeathInfo();

            for (int i = 0; i < GameMode->AliveBots.Num(); i++) {
                AFortAthenaAIBotController* Controller = GameMode->AliveBots[i];
                if (Controller == PC) {
                    GameMode->AliveBots.Remove(i);
                }
            }
            GameState->PlayerBotsLeft--;
            GameState->OnRep_PlayerBotsLeft();
        }
        else {
            if (PC->BehaviorTree->GetName().contains("NPC")) {
                for (int i = 0; i < NpcAI::NpcBots.size(); i++) {
                    if (NpcAI::NpcBots[i]->PC == PC) {
                        NpcAI::NpcBots[i]->bTickEnabled = false;
                    }
                }
            }
        }

        return;
        //return OnPossessedPawnDiedOG(PC, DamagedActor, Damage, InstigatedBy, DamageCauser, HitLocation, HitComp, BoneName, Momentum);
    }

    wchar_t* (*OnPerceptionSensedOG)(ABP_PhoebePlayerController_C* PC, AActor* SourceActor, FAIStimulus& Stimulus);
    wchar_t* OnPerceptionSensed(AFortAthenaAIBotController* PC, AActor* SourceActor, FAIStimulus& Stimulus) {
        if (!PC || !SourceActor) {
            return nullptr;
        }

        for (FortAthenaAIBotController::BotSpawnData& SpawnedBot : FortAthenaAIBotController::SpawnedBots) {
            if (!SpawnedBot.Controller || !SpawnedBot.Pawn || !SpawnedBot.PlayerState)
                continue;

            if (SpawnedBot.Controller == PC) {
                if (SpawnedBot.Controller->CurrentAlertLevel == EAlertLevel::Threatened) {
                    SpawnedBot.Controller->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_RangeAttack_ExecutionStatus"), (int)EExecutionStatus::ExecutionAllowed);
                    SpawnedBot.Controller->Blackboard->SetValueAsBool(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_ManageWeapon_Fire"), true);
                }
            }
        }
    }

    // ========== BOT FIXES INTEGRATION START ==========
    
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
                    // Use the consumable - clear blocking first
                    if (State->Pawn->AbilitySystemComponent) {
                        State->Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
                    }
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
            if (EnemyPawn->PlayerState->bIsABot) continue;
            if (EnemyPawn->GetHealth() <= 0) continue;

            FVector EnemyLocation = EnemyPawn->K2_GetActorLocation();
            float Distance = SDKUtils::Dist(BotLocation, EnemyLocation);

            if (Distance > 5000.0f) continue;

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

            float DistanceToTarget = SDKUtils::Dist(BotLocation, TargetLocation);
            if (DistanceToTarget <= 1500.0f) {
                State->Controller->Blackboard->SetValueAsBool(
                    ConvFName(L"AIEvaluator_ManageWeapon_Fire"), true
                );
                State->Pawn->PawnStartFire(0);
            } else {
                State->Controller->MoveToLocation(TargetLocation, 200.0f, true, true, false, true, nullptr, true);
            }
        } else if (!BestTarget) {
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
                // Clear any blocking states before interaction
                if (State->Pawn->AbilitySystemComponent) {
                    State->Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
                }
                
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
                // Clear blocking before pickup
                if (State->Pawn->AbilitySystemComponent) {
                    State->Pawn->AbilitySystemComponent->SetUserAbilityActivationInhibited(false);
                }
                // Pickup the item
                State->Pawn->ServerHandlePickup(Pickup, 0.3f, FVector(), true);
            } else if (Distance <= 800.0f) {
                if (Pickup->PrimaryPickupItemEntry.ItemDefinition) {
                    auto ItemDef = Pickup->PrimaryPickupItemEntry.ItemDefinition;
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
            if (State->Pawn->GetHealth() <= 0) continue;

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
    
    // ========== BOT FIXES INTEGRATION END ==========

    float (*GetTrackingModifierInternalOG)(UFortAthenaAIBotAimingDigestedSkillSet* This, int Curve, double SignNegationProbability);
    float GetTrackingModifierInternal(UFortAthenaAIBotAimingDigestedSkillSet* This, int Curve, double SignNegationProbability) {
        return 0.f;
        float TrackingModifier = GetTrackingModifierInternal(This, Curve, SignNegationProbability);
        Log("TrackingModifier: " + std::to_string(TrackingModifier));
        
        return TrackingModifier;
    }

    FDigestedWeaponAccuracy* (*GetWeaponAccuracyOG)(UFortAthenaAIBotAimingDigestedSkillSet* This, AFortWeapon* Weapon);
    FDigestedWeaponAccuracy* GetWeaponAccuracy(UFortAthenaAIBotAimingDigestedSkillSet* This, AFortWeapon* Weapon) {
        if (!This || !Weapon || !Weapon->WeaponData) return nullptr;

        static UClass* AimingSkillSet = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/Skillsets/AI_skill_phoebe_bot_aiming.AI_skill_phoebe_bot_aiming_C");

        FDigestedWeaponAccuracy* WeaponAccuracy = nullptr;

        if (AimingSkillSet) {
            UFortAthenaAIBotAimingSkillSet* AimingSkill = (UFortAthenaAIBotAimingSkillSet*)AimingSkillSet->DefaultObject;

            auto TagName = Weapon->WeaponData->AnalyticTags.GameplayTags[0].TagName;
            Log(TagName.ToString());
            if (TagName.ToString().contains("harvest")) {
                TagName = ConvFName(L"Weapon.Melee.Impact.Pickaxe");
            }

            Log("WeaponAccuraciesNum: " + std::to_string(This->WeaponAccuracies.Num()));

            for (FWeaponAccuracyCategory& WeaponAccuracyCatagory : AimingSkill->WeaponAccuracies) {
                for (FGameplayTag Tag : WeaponAccuracyCatagory.Tags.ParentTags) {
                    if (Tag.TagName.ToString().contains(TagName.ToString())) {
                        Log("Found WeaponAccuracy: Tag: " + Tag.TagName.ToString());
                        WeaponAccuracy = (FDigestedWeaponAccuracy*)&WeaponAccuracyCatagory.WeaponAccuracy;
                        break;
                    }
                }

                for (FGameplayTag Tag : WeaponAccuracyCatagory.Tags.GameplayTags) {
                    Log("Tag: " + Tag.TagName.ToString());
                    if (Tag.TagName.ToString().contains(TagName.ToString())) {
                        Log("Found WeaponAccuracy: Tag: " + Tag.TagName.ToString());
                        WeaponAccuracy = (FDigestedWeaponAccuracy*)&WeaponAccuracyCatagory.WeaponAccuracy;
                        break;
                    }
                }
            }
        }
        else {
            Log("Cant Find AimingSkillSet!");
        }

        return WeaponAccuracy;
    }

    void HookAll() {
        MH_CreateHook((LPVOID)(ImageBase + 0x2047EA4), CreateAndConfigureNavigationSystem, (LPVOID*)&CreateAndConfigureNavigationSystemOG);

        HookVTable(UAthenaNavSystem::GetDefaultObj(), 0x55, InitializeForWorld, (LPVOID*)&InitializeForWorldOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x4509720), OnPawnAISpawned, (LPVOID*)&OnPawnAISpawnedOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x46C5688), InventoryBaseOnSpawned, (LPVOID*)&InventoryBaseOnSpawnedOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x450A108), OnPossessedPawnDied, (LPVOID*)&OnPossessedPawnDiedOG);

        //MH_CreateHook((LPVOID)(ImageBase + 0x467FE94), GetTrackingModifierInternal, (LPVOID*)&GetTrackingModifierInternalOG);

        //MH_CreateHook((LPVOID)(ImageBase + 0x4680088), GetWeaponAccuracy, (LPVOID*)&GetWeaponAccuracyOG);

        // Initialize Bot Fixes (integrated directly into this namespace)
        Log("Bot Fixes Integrated!");
        
        // Initialize Quest System
        QuestSystem::HookAll();

        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogAthenaAIServiceBots VeryVerbose", nullptr);
        UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogAthenaBots VeryVerbose", nullptr);
        Log("Hooked FortAIBotControllerAthena!");
    }
}