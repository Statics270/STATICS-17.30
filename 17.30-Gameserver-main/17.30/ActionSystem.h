#pragma once
#include "framework.h"

namespace ActionSystem {
    static bool bActionSystemInitialized = false;
    
    void Initialize() {
        if (bActionSystemInitialized) return;
        bActionSystemInitialized = true;
        Log("ActionSystem Initialized!");
    }
    
    void TickActionSystem() {
        // Placeholder for action system ticking
    }
    
    void HookAll() {
        Initialize();
        Log("ActionSystem Hooked!");
    }
}
