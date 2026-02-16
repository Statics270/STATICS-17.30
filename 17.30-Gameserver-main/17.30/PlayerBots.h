#pragma once
#include "framework.h"
#include "BehaviorTreeSystem.h"

#include "BehaviorTreeDecorators.h"
#include "BehaviorTreeEvaluators.h"
#include "BehaviorTreeServices.h"
#include "BehaviorTreeTasks.h"
#include "AdvancedBotBehavior.h"

namespace PlayerBots {
    std::vector<class PhoebeBot*> PhoebeBots{};
    class PhoebeBot
    {
    public:
        // The behaviortree for the new ai system
        BehaviorTree* BT_Phoebe = nullptr;

        // The context that should be sent to the behaviortree
        BTContext Context = {};

        // The playercontroller of the bot
        AFortAthenaAIBotController* PC;

        // The Pawn of the bot
        AFortPlayerPawnAthena* Pawn;

        // The PlayerState of the bot
        AFortPlayerStateAthena* PlayerState;

        // Are we ticking the bot?
        bool bTickEnabled = true;

        // So we can track the current tick that the bot is doing
        uint64_t tick_counter = 0;

    public:
        PhoebeBot(AFortAthenaAIBotController* PC, AFortPlayerPawnAthena* Pawn, AFortPlayerStateAthena* PlayerState)
        {
            this->PC = PC;
            this->Pawn = Pawn;
            this->PlayerState = PlayerState;

            Context.Controller = PC;
            Context.Pawn = Pawn;
            Context.PlayerState = PlayerState;

            PhoebeBots.push_back(this);
        }

        bool IsPickaxeEquiped() {
            if (!Pawn || !Pawn->CurrentWeapon)
                return false;

            if (Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
            {
                return true;
            }
            return false;
        }

        void EquipPickaxe()
        {
            if (!Pawn || !Pawn->CurrentWeapon)
                return;

            if (IsPickaxeEquiped()) {
                return;
            }

            for (size_t i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
            {
                if (PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
                {
                    Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition, PC->Inventory->Inventory.ReplicatedEntries[i].ItemGuid, PC->Inventory->Inventory.ReplicatedEntries[i].TrackerGuid, false);
                    break;
                }
            }
        }

        void SwitchToWeapon() {
            if (!Pawn || !Pawn->CurrentWeapon || !Pawn->CurrentWeapon->WeaponData || !PC || !PC->Inventory)
                return;

            if (!Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
                return;
            }

            if (Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
            {
                for (size_t i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
                {
                    auto& Entry = PC->Inventory->Inventory.ReplicatedEntries[i];
                    if (Entry.ItemDefinition) {
                        if (Entry.ItemDefinition->ItemType == EFortItemType::Weapon) {
                            Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry.ItemDefinition, Entry.ItemGuid, Entry.TrackerGuid, false);
                            break;
                        }
                    }
                }
            }
        }
    };

    BehaviorTree* ConstructBehaviorTree() {
            auto* Tree = new BehaviorTree();

            auto* RootSelector = new BTComposite_Selector();
            RootSelector->NodeName = "Alive";

            {
                auto* Selector = new BTComposite_Selector();
                Selector->NodeName = "In Bus";

                {
                    auto* Task = new BTTask_Wait(999.f);
                    auto* Decorator = new BTDecorator_CheckEnum();
                    Decorator->SelectedKeyName = ConvFName(L"AIEvaluator_Global_GamePhaseStep");
                    Decorator->IntValue = (int)EAthenaGamePhaseStep::BusLocked;
                    Task->AddDecorator(Decorator);
                    Selector->AddChild(Task);
                }

                {
                    auto* Task = new BTTask_Wait(999.f);
                    auto* Decorator = new BTDecorator_CheckEnum();
                    Decorator->SelectedKeyName = ConvFName(L"AIEvaluator_Global_GamePhaseStep");
                    Decorator->IntValue = (int)EAthenaGamePhaseStep::BusFlying;
                    Task->AddDecorator(Decorator);

                    Selector->AddChild(Task);
                }

                {
                    auto* Task = new BTTask_Wait(1.f);
                    Selector->AddChild(Task);
                }

                {
                    auto* Service = new BTService_ThankBusDriver();
                    Selector->AddService(Service);
                }

                {
                    auto* Service = new BTService_JumpOffBus();
                    Selector->AddService(Service);
                }

                Tree->AllNodes.push_back(Selector);
            }

            {
                // Warmup behavior with emotes and shooting
                auto* WarmupSelector = new BTComposite_Selector();
                WarmupSelector->NodeName = "WarmupBehavior";

                {
                    auto* Decorator = new BTDecorator_CheckEnum();
                    Decorator->SelectedKeyName = ConvFName(L"AIEvaluator_Global_GamePhase");
                    Decorator->IntValue = (int)EAthenaGamePhase::Warmup;

                    auto* EmoteTask = new AdvancedBotBehavior::BTTask_PlayLobbyEmote();
                    EmoteTask->AddDecorator(Decorator);
                    WarmupSelector->AddChild(EmoteTask);
                }

                {
                    auto* Decorator = new BTDecorator_CheckEnum();
                    Decorator->SelectedKeyName = ConvFName(L"AIEvaluator_Global_GamePhase");
                    Decorator->IntValue = (int)EAthenaGamePhase::Warmup;

                    auto* ShootTask = new AdvancedBotBehavior::BTTask_LobbyShoot();
                    ShootTask->AddDecorator(Decorator);
                    WarmupSelector->AddChild(ShootTask);
                }

                {
                    auto* Task = new BTTask_Wait(1.f);
                    auto* Decorator = new BTDecorator_CheckEnum();
                    Decorator->SelectedKeyName = ConvFName(L"AIEvaluator_Global_GamePhaseStep");
                    Decorator->IntValue = (int)EAthenaGamePhaseStep::GetReady;
                    Decorator->Operator = EBlackboardCompareOp::LessThanOrEqual;
                    auto* Decorator2 = new BTDecorator_CheckEnum();
                    Decorator2->SelectedKeyName = ConvFName(L"AIEvaluator_Global_GamePhase");
                    Decorator2->IntValue = (int)EAthenaGamePhase::Warmup;
                    Task->AddDecorator(Decorator);
                    Task->AddDecorator(Decorator2);
                    WarmupSelector->AddChild(Task);
                }

                Tree->AllNodes.push_back(WarmupSelector);
                RootSelector->AddChild(WarmupSelector);
            }

            {
                auto* Task = new BTTask_RunSelector();
                Task->SelectorToRun = Tree->FindSelectorByName("In Bus");
                auto* Decorator = new BTDecorator_IsSet();
                Decorator->SelectedKeyName = ConvFName(L"AIEvaluator_Global_IsInBus");
                Task->AddDecorator(Decorator);
                if (Task->SelectorToRun) {
                    RootSelector->AddChild(Task);
                }
            }

            {
                // Combat and looting behaviors
                auto* CombatSelector = new BTComposite_Selector();
                CombatSelector->NodeName = "CombatAndLooting";

                auto* EngageTask = new AdvancedBotBehavior::BTTask_EngageEnemy();
                EngageTask->NodeName = "EngageEnemy";
                CombatSelector->AddChild(EngageTask);

                auto* LootTask = new AdvancedBotBehavior::BTTask_LootNearby();
                LootTask->NodeName = "LootNearby";
                CombatSelector->AddChild(LootTask);

                auto* BuildTask = new AdvancedBotBehavior::BTTask_BuildCover();
                BuildTask->NodeName = "BuildCover";
                CombatSelector->AddChild(BuildTask);

                Tree->AllNodes.push_back(CombatSelector);
                RootSelector->AddChild(CombatSelector);
            }

            Tree->RootNode = RootSelector;
            Tree->AllNodes.push_back(RootSelector);

            return Tree;
        }

    void TickBots() {
        for (auto bot : PhoebeBots)
        {
            if (!bot->bTickEnabled) continue;

            if (bot->BT_Phoebe) {
                bot->BT_Phoebe->Tick(bot->Context);
                bot->tick_counter++;
            }
        }
    }
}