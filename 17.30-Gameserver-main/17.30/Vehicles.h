#pragma once
#include "framework.h"

namespace Vehicles {
    static TArray<ABP_Vehicle_Radio_C*> SpawnedRadios;

    void AttachRadioToVehicle(AFortAthenaVehicle* Vehicle) {
        if (!Vehicle || !Globals::bVehicleRadioEnabled)
            return;

        // Spawn the radio actor and attach it to the vehicle
        ABP_Vehicle_Radio_C* Radio = SpawnActor<ABP_Vehicle_Radio_C>(Vehicle->K2_GetActorLocation(), Vehicle->K2_GetActorRotation(), Vehicle);

        if (Radio) {
            Radio->SetReplicates(true);
            SpawnedRadios.Add(Radio);

            // Start the radio with a random station
            if (Radio->RadioStations.Num() > 0) {
                int32 StationIndex = UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(0, Radio->RadioStations.Num() - 1);
                Radio->StartRadio(StationIndex);
            }

            Log("Attached radio to vehicle!");
        }
    }

    void SpawnVehicles() {
        auto VehicleSpawners = GetAllActorsOfClass<AFortAthenaVehicleSpawner>();

        for (auto& VehicleSpawner : VehicleSpawners) {
            if (!VehicleSpawner)
                continue;

            auto VehicleClass = VehicleSpawner->GetVehicleClass();
            if (!VehicleClass)
                continue;

            AFortAthenaVehicle* Vehicle = SpawnActor<AFortAthenaVehicle>(VehicleSpawner->K2_GetActorLocation(), VehicleSpawner->K2_GetActorRotation(), nullptr, VehicleClass);

            auto FuelComponent = Cast<UFortAthenaVehicleFuelComponent>(
                Vehicle->GetComponentByClass(UFortAthenaVehicleFuelComponent::StaticClass())
            );

            if (FuelComponent) {
                FuelComponent->ServerFuel = UKismetMathLibrary::GetDefaultObj()->RandomIntegerInRange(60, 100);
                FuelComponent->OnRep_ServerFuel(0);
            }

            if (!Vehicle)
                continue;

            // Attach radio to vehicle
            AttachRadioToVehicle(Vehicle);
        }

        VehicleSpawners.Free();
        Log("Spawned Vehicles with Radios!");
    }

    void ToggleVehicleRadio(AFortAthenaVehicle* Vehicle, bool bEnable) {
        if (!Vehicle)
            return;

        // Find radio attached to this vehicle
        for (ABP_Vehicle_Radio_C* Radio : SpawnedRadios) {
            if (Radio && Radio->GetAttachParentActor() == Vehicle) {
                if (bEnable) {
                    if (Radio->RadioStations.Num() > 0) {
                        Radio->StartRadio(Radio->RadioStationIndex);
                    }
                } else {
                    Radio->StopRadio();
                }
                break;
            }
        }
    }

    void ChangeRadioStation(AFortAthenaVehicle* Vehicle) {
        if (!Vehicle)
            return;

        for (ABP_Vehicle_Radio_C* Radio : SpawnedRadios) {
            if (Radio && Radio->GetAttachParentActor() == Vehicle) {
                if (Radio->RadioStations.Num() > 0) {
                    int32 NextStation = (Radio->RadioStationIndex + 1) % Radio->RadioStations.Num();
                    Radio->StartRadio(NextStation);
                }
                break;
            }
        }
    }
}