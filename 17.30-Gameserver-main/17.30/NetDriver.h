#pragma once
#include "framework.h"
#include "FortGameModeAthena.h"
#include "BotsSpawner.h"
#include "FortAthenaAIBotController.h"
#include "QuestSystem.h"
#include "ActionSystem.h"
#include "FortAthenaAIBotController_SDK.h"

namespace NetDriver {
    EAthenaGamePhaseStep GetCurrentGamePhaseStep(AFortGameModeAthena* GameMode, AFortGameStateAthena* GameState) {
        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        if (GameState->GamePhase == EAthenaGamePhase::Setup) {
            return EAthenaGamePhaseStep::Setup;
        }
        else if (GameState->GamePhase == EAthenaGamePhase::Warmup) {
            if (GameState->WarmupCountdownEndTime > CurrentTime + 10.f) {
                return EAthenaGamePhaseStep::Warmup;
            }
            else {
                return EAthenaGamePhaseStep::GetReady;
            }
        }
        else if (GameState->GamePhase == EAthenaGamePhase::Aircraft) {
            if (!GameState->bAircraftIsLocked) {
                return EAthenaGamePhaseStep::BusFlying;
            }
            else {
                return EAthenaGamePhaseStep::BusLocked;
            }
        }
        else if (GameState->GamePhase == EAthenaGamePhase::SafeZones) {
            if (!GameState->SafeZoneIndicator) {
                return EAthenaGamePhaseStep::StormForming;
            }
            else if (GameState->SafeZoneIndicator->bPaused) {
                return EAthenaGamePhaseStep::StormHolding;
            }
            else {
                return EAthenaGamePhaseStep::StormShrinking;
            }
        }
        else if (GameState->GamePhase == EAthenaGamePhase::EndGame) {
            return EAthenaGamePhaseStep::EndGame;
        }
        else if (GameState->GamePhase == EAthenaGamePhase::Count) {
            return EAthenaGamePhaseStep::Count;
        }
        else {
            return EAthenaGamePhaseStep::EAthenaGamePhaseStep_MAX;
        }
    }

    void UpdateBotBlackboard(AFortAthenaAIBotController* bot, AFortGameModeAthena* GameMode, AFortGameStateAthena* GameState) {
        if (!bot) return;
        if (!bot->Blackboard) return;
        if (!GameState) return;

        bot->Blackboard->SetValueAsEnum(UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(L"AIEvaluator_Global_GamePhaseStep"), (int)GameState->GamePhaseStep);
    }

    void (*TickFlushOG)(UNetDriver* This, float DeltaSeconds);
    void TickFlush(UNetDriver* This, float DeltaSeconds) {
        if (!This)
            return;

        float CurrentTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());

        AFortGameModeAthena* GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
        AFortGameStateAthena* GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

        if (This->ClientConnections.Num() > 0) {
            ServerReplicateActors(This->ReplicationDriver, DeltaSeconds);
        }

        if (GameMode && GameState) {
            EAthenaGamePhaseStep CurrentGamePhaseStep = GetCurrentGamePhaseStep(GameMode, GameState);
            if (CurrentGamePhaseStep != GameState->GamePhaseStep) {
                GameState->GamePhaseStep = CurrentGamePhaseStep;
                
                // Notify quest system of game phase change
                QuestSystem::OnGamePhaseChanged(GameState->GamePhase);
                
                if (Globals::bBotsEnabled && !Globals::bBotsShouldUseManualTicking) {
                    if (FortAthenaAIBotController::SpawnedBots.size() != 0) {
                        for (int i = 0; i < FortAthenaAIBotController::SpawnedBots.size(); i++) {
                            AFortAthenaAIBotController* PC = FortAthenaAIBotController::SpawnedBots[i].Controller;
                            if (!PC)
                                continue;

                            UpdateBotBlackboard(PC, GameMode, GameState);
                        }
                    }
                }
            }
        }

        // === OPTIMIZED SPAWNING SYSTEM ===
        if (GameState->WarmupCountdownEndTime - UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) <= 0 && GameState->GamePhase == EAthenaGamePhase::Warmup)
        {
            FortGameModeAthena::StartAircraftPhase(GameMode, 0);
        }

        // Spawn bots one by one during warmup - distributed spawning
        if (GameState->GamePhase == EAthenaGamePhase::Warmup &&
            GameMode->AlivePlayers.Num() > 0
            && (GameMode->AlivePlayers.Num() + GameMode->AliveBots.Num()) < GameMode->GameSession->MaxPlayers
            && GameMode->AliveBots.Num() < Globals::MaxBotsToSpawn && Globals::bBotsEnabled)
        {
            static float LastWarmupBotSpawn = 0.0f;
            if (CurrentTime - LastWarmupBotSpawn >= 0.1f) { // One bot every 0.1 seconds
                LastWarmupBotSpawn = CurrentTime;
                BotsSpawner::SpawnPlayerBot();
            }
        }

        // Spawn bots one by one during aircraft - distributed spawning
        if ((GameState->GamePhase == EAthenaGamePhase::Aircraft || GameState->GamePhase == EAthenaGamePhase::SafeZones)
            && GameMode->AlivePlayers.Num() > 0
            && (GameMode->AlivePlayers.Num() + GameMode->AliveBots.Num()) < GameMode->GameSession->MaxPlayers
            && GameMode->AliveBots.Num() < Globals::MaxBotsToSpawn && Globals::bBotsEnabled)
        {
            static float LastAircraftBotSpawn = 0.0f;
            if (CurrentTime - LastAircraftBotSpawn >= 0.1f) { // One bot every 0.1 seconds
                LastAircraftBotSpawn = CurrentTime;
                BotsSpawner::SpawnPlayerBot();
            }
        }

        // === OPTIMIZED TICK SYSTEM - DISTRIBUTED PROCESSING ===
        if (Globals::bBotsEnabled) {
            // Bot tick - only every 0.1 seconds
            static float LastBotTick = 0.0f;
            if (CurrentTime - LastBotTick >= 0.1f) {
                LastBotTick = CurrentTime;
                
                if (Globals::bBotsShouldUseManualTicking) {
                    NpcAI::TickBots();
                }
                else {
                    NpcAI::TickBehaviorTree();
                }
                PlayerBots::TickBots();
                
                // Tick bot fixes for improved AI behavior (integrated in FortAthenaAIBotController)
                FortAthenaAIBotController::TickBotFixes();
                
                // Tick new bot system SDK
                FortAthenaAIBotController_SDK::TickBotSystem();
            }
        }

        // Action system tick - 20 times per second (every 0.05s)
        static float LastActionTick = 0.0f;
        if (CurrentTime - LastActionTick >= 0.05f) {
            LastActionTick = CurrentTime;
            ActionSystem::TickActionSystem();
        }

        // Tick quest system for time-based updates
        QuestSystem::TickSurvivalXP();

        return TickFlushOG(This, DeltaSeconds);
    }

    float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing = true) {
        return 30.f;
    }

    int GetNetMode()
    {
        return (int)ENetMode::DedicatedServer;
    }

    void HookAll() {
        MH_CreateHook((LPVOID)(ImageBase + 0xE4043C), TickFlush, (LPVOID*)&TickFlushOG);

        MH_CreateHook((LPVOID)(ImageBase + 0x104914C), GetMaxTickRate, nullptr);

        MH_CreateHook((LPVOID)(ImageBase + 0xCD2164), GetNetMode, nullptr); // UWorld::GetNEtMode
        MH_CreateHook((LPVOID)(ImageBase + 0xE44930), GetNetMode, nullptr); // AActor::GetNetMode
        MH_CreateHook((LPVOID)(ImageBase + 0xE433AC), GetNetMode, nullptr); // UNetDriver::GetNetMode

        Log("NetDriver Hooked!");
    }
}