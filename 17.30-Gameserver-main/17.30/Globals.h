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

    //REAL PLAYERS
    static int NextTeamIndex = 0;
    static int CurrentPlayersOnTeam = 0;
    static int MaxPlayersPerTeam = 1;
}
