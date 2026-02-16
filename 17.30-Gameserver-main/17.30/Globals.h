#pragma once

namespace Globals {
    bool bIsProdServer = false;

    bool bCreativeEnabled = false;
    bool bSTWEnabled = false;
    bool bEventEnabled = false;

    bool bBotsEnabled = true;
    bool bBotsShouldUseManualTicking = false;

    int MaxBotsToSpawn = 95;
    int MinPlayersForEarlyStart = 95;

    // Advanced Bot Behavior Options
    bool bPOIBasedSpawning = true;
    bool bBotLootingEnabled = true;
    bool bBotCombatEnabled = true;
    bool bBotBuildingEnabled = true;
    bool bBotEmotesEnabled = true;
    bool bBotLobbyShooting = true;

    // Vehicle Options
    bool bVehicleRadioEnabled = true;

    // Bot Fixes - AI Behavior Improvements
    bool bBotFixesEnabled = true;           // Enable bot movement, combat, and action fixes
    bool bBotBusJumpFix = true;             // Enable proper bot bus jumping
    bool bBotDamageFix = true;              // Enable bot damage taking
    bool bBotStuckDetection = true;         // Enable bot stuck detection and unstuck
    
    // Quest System - C2S7 Integration
    bool bQuestSystemEnabled = true;        // Enable C2S7 quest system
    bool bDailyQuestsEnabled = true;        // Enable daily quests
    bool bWeeklyQuestsEnabled = true;       // Enable weekly quests (C2S7 Invasion)
    bool bBattlePassQuestsEnabled = true;   // Enable battle pass challenges
    bool bXPAwardsEnabled = true;           // Enable XP awards for actions

    // Action Blocking Fixes
    bool bConsumablesFix = true;            // Enable consumables action fix
    bool bReloadFix = true;                 // Enable reload action fix
    bool bGrenadesFix = true;               // Enable grenades action fix
    bool bInteractFix = true;               // Enable interact key fix
    bool bChestsFix = true;                 // Enable chest opening fix
    bool bHarvestingFix = true;             // Enable harvesting fix

    //REAL PLAYERS
    static int NextTeamIndex = 0;
    static int CurrentPlayersOnTeam = 0;
    static int MaxPlayersPerTeam = 1;
}
