#pragma once
#include "framework.h"

namespace QuestSystem {
    // C2S7 Quest System - Battle Pass and Weekly Challenges
    
    enum class EQuestType : uint8_t {
        Daily,
        Weekly,
        BattlePass,
        Event,
        Special
    };

    enum class EQuestObjectiveType : uint8_t {
        Eliminations,
        DamageDealt,
        ChestsOpened,
        GamesPlayed,
        VictoryRoyales,
        Harvesting,
        Building,
        DistanceTraveled,
        ConsumablesUsed,
        WeaponsCollected,
        POIVisited
    };

    struct FQuestObjective {
        EQuestObjectiveType Type;
        int TargetValue;
        int CurrentValue;
        FName StatTag;
        bool bCompleted;
    };

    struct FQuestData {
        FString QuestID;
        FString DisplayName;
        FString Description;
        EQuestType Type;
        int WeekNumber;
        int SeasonNumber;
        int XPReward;
        int BattleStarsReward;
        TArray<FQuestObjective> Objectives;
        bool bCompleted;
        bool bClaimed;
    };

    // C2S7 Weekly Quests (Invasion Season)
    static TArray<FQuestData> WeeklyQuests;
    static TArray<FQuestData> DailyQuests;
    static TArray<FQuestData> BattlePassQuests;

    // XP Rewards for different actions
    static const int XP_PER_ELIMINATION = 150;
    static const int XP_PER_ASSIST = 75;
    static const int XP_VICTORY_ROYALE = 2500;
    static const int XP_TOP_10 = 500;
    static const int XP_TOP_25 = 250;
    static const int XP_CHEST_OPENED = 130;
    static const int XP_AMMO_BOX_OPENED = 85;
    static const int XP_HARVESTING = 10;
    static const int XP_SURVIVAL_MINUTE = 17;
    static const int XP_FIRST_BLOOD = 150;

    // Player quest progress tracking
    struct FPlayerQuestProgress {
        AFortPlayerControllerAthena* Controller;
        AFortPlayerStateAthena* PlayerState;
        TMap<FString, int> ObjectiveProgress;
        int TotalXPThisMatch;
        int SurvivalStartTime;
        bool bFirstBlood;
        bool bInBus;
        bool bHasLanded;
    };

    static TArray<FPlayerQuestProgress*> PlayerProgress;

    void InitializeQuestSystem();
    void InitializeWeeklyQuests();
    void InitializeDailyQuests();
    void InitializeBattlePassQuests();
    
    FPlayerQuestProgress* GetOrCreatePlayerProgress(AFortPlayerControllerAthena* Controller);
    void UpdateQuestProgress(AFortPlayerControllerAthena* Controller, EQuestObjectiveType ObjectiveType, int Amount = 1);
    void AwardXP(AFortPlayerControllerAthena* Controller, int XPAmount, FString Reason);
    void CheckQuestCompletion(FPlayerQuestProgress* Progress, FQuestData& Quest);
    void OnPlayerElimination(AFortPlayerControllerAthena* Killer, AFortPlayerControllerAthena* Victim, bool bIsElimination);
    void OnChestOpened(AFortPlayerControllerAthena* Controller);
    void OnHarvesting(AFortPlayerControllerAthena* Controller);
    void OnGamePhaseChanged(EAthenaGamePhase NewPhase);
    void TickSurvivalXP();
    void CleanupPlayerProgress(AFortPlayerControllerAthena* Controller);

    // Initialize C2S7 quests
    void InitializeQuestSystem() {
        Log("Initializing C2S7 Quest System...");
        InitializeWeeklyQuests();
        InitializeDailyQuests();
        InitializeBattlePassQuests();
        Log("Quest System Initialized!");
    }

    void InitializeWeeklyQuests() {
        // Week 1 Quests
        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_C2S7_W1_1";
            Quest.DisplayName = L"First Contact";
            Quest.Description = L"Eliminate Alien Invaders";
            Quest.Type = EQuestType::Weekly;
            Quest.WeekNumber = 1;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 30000;
            Quest.BattleStarsReward = 5;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::Eliminations;
            Obj.TargetValue = 5;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"alien_eliminations");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            WeeklyQuests.Add(Quest);
        }

        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_C2S7_W1_2";
            Quest.DisplayName = L"Resource Gathering";
            Quest.Description = L"Harvest building materials";
            Quest.Type = EQuestType::Weekly;
            Quest.WeekNumber = 1;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 24000;
            Quest.BattleStarsReward = 3;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::Harvesting;
            Obj.TargetValue = 1000;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"materials_harvested");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            WeeklyQuests.Add(Quest);
        }

        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_C2S7_W1_3";
            Quest.DisplayName = L"Treasure Hunter";
            Quest.Description = L"Open chests at different Named Locations";
            Quest.Type = EQuestType::Weekly;
            Quest.WeekNumber = 1;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 24000;
            Quest.BattleStarsReward = 3;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::ChestsOpened;
            Obj.TargetValue = 10;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"chests_opened");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            WeeklyQuests.Add(Quest);
        }

        // Week 2 Quests
        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_C2S7_W2_1";
            Quest.DisplayName = L"Defense Protocol";
            Quest.Description = L"Deal damage to opponents";
            Quest.Type = EQuestType::Weekly;
            Quest.WeekNumber = 2;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 30000;
            Quest.BattleStarsReward = 5;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::DamageDealt;
            Obj.TargetValue = 5000;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"damage_dealt");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            WeeklyQuests.Add(Quest);
        }
    }

    void InitializeDailyQuests() {
        // Daily Quests Pool
        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_Daily_Eliminations";
            Quest.DisplayName = L"Daily Target Practice";
            Quest.Description = L"Eliminate opponents";
            Quest.Type = EQuestType::Daily;
            Quest.WeekNumber = 0;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 10000;
            Quest.BattleStarsReward = 1;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::Eliminations;
            Obj.TargetValue = 3;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"daily_eliminations");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            DailyQuests.Add(Quest);
        }

        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_Daily_Survival";
            Quest.DisplayName = L"Daily Survivor";
            Quest.Description = L"Outlast opponents";
            Quest.Type = EQuestType::Daily;
            Quest.WeekNumber = 0;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 8000;
            Quest.BattleStarsReward = 1;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::GamesPlayed;
            Obj.TargetValue = 75;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"players_outlasted");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            DailyQuests.Add(Quest);
        }
    }

    void InitializeBattlePassQuests() {
        // Battle Pass Challenges
        {
            FQuestData Quest;
            Quest.QuestID = L"Quest_BP_C2S7_Level1";
            Quest.DisplayName = L"Invasion Begins";
            Quest.Description = L"Complete Invasion challenges";
            Quest.Type = EQuestType::BattlePass;
            Quest.WeekNumber = 0;
            Quest.SeasonNumber = 17;
            Quest.XPReward = 50000;
            Quest.BattleStarsReward = 10;

            FQuestObjective Obj;
            Obj.Type = EQuestObjectiveType::VictoryRoyales;
            Obj.TargetValue = 1;
            Obj.CurrentValue = 0;
            Obj.StatTag = UKismetStringLibrary::Conv_StringToName(L"victory_royales");
            Obj.bCompleted = false;
            Quest.Objectives.Add(Obj);

            BattlePassQuests.Add(Quest);
        }
    }

    FPlayerQuestProgress* GetOrCreatePlayerProgress(AFortPlayerControllerAthena* Controller) {
        if (!Controller) return nullptr;

        for (auto* Progress : PlayerProgress) {
            if (Progress && Progress->Controller == Controller) {
                return Progress;
            }
        }

        FPlayerQuestProgress* NewProgress = new FPlayerQuestProgress();
        NewProgress->Controller = Controller;
        NewProgress->PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
        NewProgress->TotalXPThisMatch = 0;
        NewProgress->SurvivalStartTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        NewProgress->bFirstBlood = false;
        NewProgress->bInBus = true;
        NewProgress->bHasLanded = false;
        PlayerProgress.Add(NewProgress);

        return NewProgress;
    }

    void AwardXP(AFortPlayerControllerAthena* Controller, int XPAmount, FString Reason) {
        if (!Controller || !Controller->XPComponent) return;

        auto* Progress = GetOrCreatePlayerProgress(Controller);
        if (Progress) {
            Progress->TotalXPThisMatch += XPAmount;
        }

        // Use the XP component to award XP
        Controller->XPComponent->bRegisteredWithQuestManager = true;

        // Update season level display
        AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)Controller->PlayerState;
        if (PlayerState) {
            PlayerState->SeasonLevelUIDisplay = Controller->XPComponent->CurrentLevel;
            PlayerState->OnRep_SeasonLevelUIDisplay();
        }

        Log("Awarded " + std::to_string(XPAmount) + " XP to player for: " + Reason.ToString());
    }

    void OnPlayerElimination(AFortPlayerControllerAthena* Killer, AFortPlayerControllerAthena* Victim, bool bIsElimination) {
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        if (Killer && bIsElimination) {
            auto* KillerProgress = GetOrCreatePlayerProgress(Killer);

            // Award elimination XP
            AwardXP(Killer, XP_PER_ELIMINATION, FString(L"Elimination"));

            // Check for first blood
            if (!KillerProgress->bFirstBlood) {
                KillerProgress->bFirstBlood = true;
                AwardXP(Killer, XP_FIRST_BLOOD, FString(L"First Blood"));
            }

            // Update quest progress
            UpdateQuestProgress(Killer, EQuestObjectiveType::Eliminations);
        }

        // Award assist XP if applicable (simplified - would need damage tracking)
        if (Victim) {
            CleanupPlayerProgress(Victim);
        }
    }

    void UpdateQuestProgress(AFortPlayerControllerAthena* Controller, EQuestObjectiveType ObjectiveType, int Amount) {
        if (!Controller) return;

        auto* Progress = GetOrCreatePlayerProgress(Controller);
        if (!Progress) return;

        // Update weekly quests
        for (auto& Quest : WeeklyQuests) {
            if (Quest.bCompleted) continue;
            
            for (auto& Objective : Quest.Objectives) {
                if (Objective.Type == ObjectiveType && !Objective.bCompleted) {
                    Objective.CurrentValue += Amount;
                    if (Objective.CurrentValue >= Objective.TargetValue) {
                        Objective.CurrentValue = Objective.TargetValue;
                        Objective.bCompleted = true;
                        CheckQuestCompletion(Progress, Quest);
                    }
                }
            }
        }

        // Update daily quests
        for (auto& Quest : DailyQuests) {
            if (Quest.bCompleted) continue;
            
            for (auto& Objective : Quest.Objectives) {
                if (Objective.Type == ObjectiveType && !Objective.bCompleted) {
                    Objective.CurrentValue += Amount;
                    if (Objective.CurrentValue >= Objective.TargetValue) {
                        Objective.CurrentValue = Objective.TargetValue;
                        Objective.bCompleted = true;
                        CheckQuestCompletion(Progress, Quest);
                    }
                }
            }
        }
    }

    void CheckQuestCompletion(FPlayerQuestProgress* Progress, FQuestData& Quest) {
        if (!Progress) return;

        bool bAllObjectivesComplete = true;
        for (auto& Objective : Quest.Objectives) {
            if (!Objective.bCompleted) {
                bAllObjectivesComplete = false;
                break;
            }
        }

        if (bAllObjectivesComplete && !Quest.bCompleted) {
            Quest.bCompleted = true;
            FString Reason = FString(L"Quest Completed: ");
            Reason += Quest.DisplayName;
            AwardXP(Progress->Controller, Quest.XPReward, Reason);
            Log("Quest Completed: " + Quest.DisplayName.ToString());
        }
    }

    void OnChestOpened(AFortPlayerControllerAthena* Controller) {
        if (!Controller) return;

        AwardXP(Controller, XP_CHEST_OPENED, FString(L"Chest Opened"));
        UpdateQuestProgress(Controller, EQuestObjectiveType::ChestsOpened);
    }

    void OnHarvesting(AFortPlayerControllerAthena* Controller) {
        if (!Controller) return;

        AwardXP(Controller, XP_HARVESTING, FString(L"Harvesting"));
        UpdateQuestProgress(Controller, EQuestObjectiveType::Harvesting);
    }

    void OnGamePhaseChanged(EAthenaGamePhase NewPhase) {
        if (NewPhase == EAthenaGamePhase::Warmup) {
            // Reset player progress for new match
            for (auto* Progress : PlayerProgress) {
                if (Progress) {
                    Progress->SurvivalStartTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
                    Progress->bInBus = true;
                    Progress->bHasLanded = false;
                }
            }
        }
    }

    void TickSurvivalXP() {
        static float LastSurvivalTick = 0;
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
        
        if (CurrentTime - LastSurvivalTick < 60.0f) return; // Every minute
        LastSurvivalTick = CurrentTime;

        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
        if (!GameState || GameState->GamePhase != EAthenaGamePhase::SafeZones) return;

        for (auto* Progress : PlayerProgress) {
            if (Progress && Progress->Controller && Progress->Controller->MyFortPawn) {
                if (Progress->Controller->MyFortPawn->GetHealth() > 0) {
                    AwardXP(Progress->Controller, XP_SURVIVAL_MINUTE, FString(L"Survival Time"));
                }
            }
        }
    }

    void CleanupPlayerProgress(AFortPlayerControllerAthena* Controller) {
        for (int i = 0; i < PlayerProgress.Num(); i++) {
            if (PlayerProgress[i] && PlayerProgress[i]->Controller == Controller) {
                delete PlayerProgress[i];
                PlayerProgress.Remove(i);
                break;
            }
        }
    }

    void OnPlayerWonVictoryRoyale(AFortPlayerControllerAthena* Controller) {
        if (!Controller) return;

        AwardXP(Controller, XP_VICTORY_ROYALE, FString(L"Victory Royale!"));
        UpdateQuestProgress(Controller, EQuestObjectiveType::VictoryRoyales);

        // Check for placement XP
        AwardXP(Controller, XP_TOP_10, FString(L"Top 10 Finish"));
    }

    void HookAll() {
        InitializeQuestSystem();
        Log("QuestSystem Hooked!");
    }
}
