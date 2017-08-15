#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\MeatAIModule.h"
# include "Source\InventoryManager.h"

using namespace BWAPI;


// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
bool MeatAIModule::Tech_Avail() {

    bool avail = false;

    for ( auto & u : BWAPI::Broodwar->self()->getUnits() ) {

        if ( u->getType() == BWAPI::UnitTypes::Zerg_Drone ) {
            bool long_condition = (u->canBuild( BWAPI::UnitTypes::Zerg_Spawning_Pool ) && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Evolution_Chamber ) && Count_Units( BWAPI::UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) < 2) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Hydralisk_Den ) && Count_Units( BWAPI::UnitTypes::Zerg_Hydralisk_Den, friendly_inventory ) == 0)||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Spire ) && Count_Units( BWAPI::UnitTypes::Zerg_Spire, friendly_inventory ) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Queens_Nest ) && Count_Units( BWAPI::UnitTypes::Zerg_Queens_Nest, friendly_inventory ) == 0 && Count_Units( BWAPI::UnitTypes::Zerg_Spire, friendly_inventory ) > 0 ) || // I have hardcoded spire before queens nest.
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Ultralisk_Cavern ) && Count_Units( BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) == 0);
            if ( long_condition ) {
                avail = true;
            }
        }
        else if ( u->getType().isSuccessorOf(BWAPI::UnitTypes::Zerg_Hatchery) && !u->isUpgrading() && !u->isMorphing() ) {
            bool long_condition = (u->canMorph( BWAPI::UnitTypes::Zerg_Lair ) && Count_Units( BWAPI::UnitTypes::Zerg_Lair, friendly_inventory ) == 0 && Count_Units( BWAPI::UnitTypes::Zerg_Hive, friendly_inventory ) == 0) ||
                    (u->canMorph( BWAPI::UnitTypes::Zerg_Hive ) && Count_Units( BWAPI::UnitTypes::Zerg_Hive, friendly_inventory ) == 0);
            if ( long_condition ) {
                avail = true;
            }
        }
        else if ( u->getType().isBuilding() && !u->isUpgrading() && !u->isMorphing() ){ // check idle buildings for potential upgrades.
            for ( int i = 0; i != 12; i++ )
            { // iterating through the main upgrades we have available and MeatAI "knows" about. 
                int known_ups[11] = { 3, 10, 11, 25, 26, 27, 28, 29, 30, 52, 53 }; // Identifies zerg upgrades of that we have initialized at this time. See UpgradeType definition for references, listed below for conveinence.
                BWAPI::UpgradeType up_current = (BWAPI::UpgradeType) known_ups[i];
                BWAPI::UpgradeType::set building_up_set = u->getType().upgradesWhat(); // does this idle building make that upgrade?
                if ( building_up_set.find( up_current ) != building_up_set.end() ) {
                    if ( BWAPI::Broodwar->self()->getUpgradeLevel( up_current ) < up_current.maxRepeats() && !BWAPI::Broodwar->self()->isUpgrading( up_current ) ) { // if it is not maxed, and nothing is upgrading it, then there must be some tech work we could do.
                        avail = true;
                    }
                }
            }
        } // if condition
    }// for every unit

    return avail;
}
// Tells a building to begin the next tech on our list.
void MeatAIModule::Tech_Begin(Unit building, const Unit_Inventory &ui) {

    Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, building, tech_starved );

    Check_N_Upgrade( UpgradeTypes::Metabolic_Boost, building, tech_starved && ( Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Missile_Attacks ) == 3 ) );
    Check_N_Upgrade( UpgradeTypes::Zerg_Melee_Attacks, building, tech_starved && ( Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Missile_Attacks ) == 3 ) );

    Check_N_Upgrade( UpgradeTypes::Zerg_Missile_Attacks, building, tech_starved && (Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3) );
    Check_N_Upgrade( UpgradeTypes::Muscular_Augments, building, tech_starved && (Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3 ) );
    Check_N_Upgrade( UpgradeTypes::Grooved_Spines, building, tech_starved && (Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3 ) );

    Check_N_Build( UnitTypes::Zerg_Lair, building, ui, tech_starved &&
        Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) == 0 && //don't need lair if we have a lair
        Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) == 0 && //don't need lair if we have a hive.
        building->getType() == UnitTypes::Zerg_Hatchery );

    Check_N_Upgrade( UpgradeTypes::Pneumatized_Carapace, building, tech_starved && (Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) > 0 || Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) > 0) );
    Check_N_Upgrade( UpgradeTypes::Antennae, building, tech_starved && (Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) > 0 || Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) > 0 ) ); //don't need lair if we have a hive.

    Check_N_Build( UnitTypes::Zerg_Hive, building, ui, tech_starved &&
        Count_Units( UnitTypes::Zerg_Queens_Nest, friendly_inventory ) >= 0 &&
        building->getType() == UnitTypes::Zerg_Lair &&
        Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) == 0 ); //If you're tech-starved at this point, don't make random hives.

    Check_N_Upgrade( UpgradeTypes::Adrenal_Glands, building, tech_starved && Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) > 0 );
    Check_N_Upgrade( UpgradeTypes::Anabolic_Synthesis, building, tech_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) > 0 );
    Check_N_Upgrade( UpgradeTypes::Chitinous_Plating, building, tech_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) > 0 );
}