#pragma once

#include "MeatAIModule.h"
#include "CobbDouglas.h"
#include "InventoryManager.h"
#include "Unit_Inventory.h"
#include "GeneticHistoryManager.h"
#include "Fight_MovementManager.h"
#include "AssemblyManager.h"
#include <iostream> 
#include <fstream> // for file read/writing
#include <chrono> // for in-game frame clock.

// MeatAI V1.00. Current V goal-> Clean up issues.
// Unresolved issue: Text coloration in on screen messages. Screen messages doesn't take color commands, but does take printf format commands. Gets messy very fast.
// Scourge have tendancy to overpopulate.
// workers are pulled back to closest in the middle of a transfer.
// geyser logic is a little wonky. Check fastest map for demonstration.
// rearange units perpendicular to opponents for instant concaves.
// Mineral Locking.
// Update 
// reserve locations for buildings.
// complete removal of vision from CD and put as part of knee-jerk responses.
// units may die from burning down, extractors, or mutations. Would cause confusion in inventory system.
// lurkers, guardians and remaining tech units.



// Bugs and goals.
// extractor trick.

using namespace BWAPI;
using namespace Filter;
using namespace std;


void MeatAIModule::onStart()
{

  // Hello World!
    Broodwar->sendText( "Hello world! This is MeatShieldAI V1.10" );

    // Print the map name.
    // BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
    Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

    // Enable the UserInput flag, which allows us to control the bot and type messages.
    Broodwar->enableFlag( Flag::UserInput );

    // Uncomment the following line and the bot will know about everything through the fog of war (cheat).
    //Broodwar->enableFlag(Flag::CompleteMapInformation);

    // Set the command optimization level so that common commands can be grouped
    // and reduce the bot's APM (Actions Per Minute).
    Broodwar->setCommandOptimizationLevel( 2 );

    // Check if this is a replay
    if ( Broodwar->isReplay() )
    {
        // Announce the players in the replay
        Broodwar << "The following players are in this replay:" << std::endl;

        // Iterate all the players in the game using a std:: iterator
        Playerset players = Broodwar->getPlayers();
        for ( auto p : players )
        {
            // Only print the player if they are not an observer
            if ( !p->isObserver() )
                Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
        }

    }
    else // if this is not a replay
    {
        // Retrieve you and your enemy's races. enemy() will just return the first enemy.
        // If you wish to deal with multiple enemies then you must use enemies().
        if ( Broodwar->enemy() ) // First make sure there is an enemy
            Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
    }

    //Initialize state variables
    gas_starved = false;
    army_starved = false;
    supply_starved = false;
    vision_starved = false;
    econ_starved = true;
    tech_starved = false;

    //Initialize model variables. 
    GeneticHistory gene_history = GeneticHistory( "output.csv" );
    delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
    gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

    //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
    alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
    alpha_vis =  gene_history.a_vis_out_mutate_; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant. Currently defunct.
    alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
    alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
    win_rate = (1 - gene_history.loss_rate_) == 1 ? 0 : (1 - gene_history.loss_rate_);

    //update Map Knowledge
    inventory.updateMineralPos();
    inventory.updateBuildablePos();
    inventory.updateBaseLoc();
    inventory.updateSmoothPos();
    inventory.updateBaseLoc();

}

void MeatAIModule::onEnd( bool isWinner )
{// Called when the game ends
   ofstream output; // Prints to brood war file proper.
   output.open( "output.csv", ios_base::app );
   //output << "delta (gas)" << "," << "gamma (supply)" << ',' << "alpha_army" << ',' << "alpha_vis" << ',' << "alpha_econ" << ',' << "alpha_tech" << ',' << "Race" << "," << "Won" << "Seed" << endl;
   output << delta  << "," << gamma << ',' << alpha_army << ',' << alpha_vis << ',' << alpha_econ << ',' << alpha_tech << ',' << Broodwar->enemy()->getRace() << "," << isWinner << ',' << short_delay << ',' << med_delay << ',' << long_delay << ',' << Broodwar->getRandomSeed() << endl;
   output.close();
}

void MeatAIModule::onFrame()
{ // Called once every game frame

  // Return if the game is a replay or is paused
    if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
        return;

        // Start Game clock.
        // Performance Qeuery Timer
        // http://www.decompile.com/cpp/faq/windows_timer_api.htm

    std::chrono::duration<double, std::milli> preamble_time;
    std::chrono::duration<double, std::milli> larva_time;
    std::chrono::duration<double, std::milli> worker_time;
    std::chrono::duration<double, std::milli> scout_time;
    std::chrono::duration<double, std::milli> combat_time;
    std::chrono::duration<double, std::milli> detector_time;
    std::chrono::duration<double, std::milli> upgrade_time;
    std::chrono::duration<double, std::milli> creepcolony_time;
    std::chrono::duration<double, std::milli> total_frame_time; //will use preamble start time.

        auto start_preamble = std::chrono::high_resolution_clock::now();
        // Assess enemy stock and general positions.

        // Let us see what is stored in each unit_inventory and update it. Invalidate unwanted units. Most notably, geysers become extractors on death.

        for ( auto e = enemy_inventory.unit_inventory_.begin(); e != enemy_inventory.unit_inventory_.end() && !enemy_inventory.unit_inventory_.empty(); e++ ) {
            if ( (*e).second.bwapi_unit_ && (*e).second.bwapi_unit_->exists() ) { // If the unit is visible now, update its position.
                (*e).second.pos_ = (*e).second.bwapi_unit_->getPosition();
                (*e).second.type_ = (*e).second.bwapi_unit_->getType();
                (*e).second.current_hp_ = (*e).second.bwapi_unit_->getHitPoints();
                (*e).second.valid_pos_ = true;
                //Broodwar->sendText( "Relocated a %s.", (*e).second.type_.c_str() );
            }
            else if ( Broodwar->isVisible( { e->second.pos_.x / 32 , e->second.pos_.y / 32 } ) && e->second.type_.canMove() ) {  // if you can see the tile it SHOULD be at and it might move... Burned down buildings will pose a problem in future.

                bool present = false;

                Unitset enemies_tile = Broodwar->getUnitsOnTile( { e->second.pos_.x / 32, e->second.pos_.y / 32 }, IsEnemy || IsNeutral );  // Confirm it is present.  Some addons convert to neutral if their main base disappears.
                for ( auto et = enemies_tile.begin(); et != enemies_tile.end(); ++et ) {
                    present = (*et)->getID() != e->second.unit_ID_ || (*et)->isCloaked() || (*et)->isBurrowed();
                    if ( present ) {
                        break;
                    }
                }
                if ( !present || enemies_tile.empty() ) { // If the last known position is visible, and the unit is not there, then they have an unknown position.  Note a variety of calls to e->first cause crashes here. 
                    e->second.valid_pos_ = false;
                    //Broodwar->sendText( "Lost track of a %s.", e->second.type_.c_str() );
                }
            }

            if ( e->second.type_ == UnitTypes::Resource_Vespene_Geyser ) { // Destroyed refineries revert to geyers, requiring the manual catch 
                e->second.valid_pos_ = false;
            }

            if ( _ANALYSIS_MODE && e->second.valid_pos_ == true ) {
                if ( isOnScreen( e->second.pos_ ) ) {
                    Broodwar->drawCircleMap( e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red ); // Plot their last known position.
                }
            }
        }

        for ( auto e = enemy_inventory.unit_inventory_.begin(); e != enemy_inventory.unit_inventory_.end() && !enemy_inventory.unit_inventory_.empty(); ) {
            if ( e->second.type_ == UnitTypes::Resource_Vespene_Geyser || // Destroyed refineries revert to geyers, requiring the manual catc.
                e->second.type_ == UnitTypes::None ) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
                e = enemy_inventory.unit_inventory_.erase( e ); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
            }
            else {
                ++e;
            }
        }

        Unitset enemy_set_all = getUnit_Set( enemy_inventory, { 0,0 }, 999999 ); // for allin mode.

        // easy to update friendly unit inventory.
        friendly_inventory = Unit_Inventory( Broodwar->self()->getUnits() );
        for ( auto f = friendly_inventory.unit_inventory_.begin(); f != friendly_inventory.unit_inventory_.end() && !friendly_inventory.unit_inventory_.empty(); ) {
            if ( f->second.type_ == UnitTypes::Resource_Vespene_Geyser || // Destroyed refineries revert to geyers, requiring the manual catc.
                f->second.type_ == UnitTypes::None ) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
                f = friendly_inventory.unit_inventory_.erase( f ); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
            }
            else {
                ++f;
            }
        }

        //Update posessed minerals. Erase those that are mined out.
        for ( auto r = inventory.resource_positions_.begin(); r != inventory.resource_positions_.end() && !inventory.resource_positions_.empty(); ) {

            TilePosition tile_pos = { r->x / 32, r->y / 32 };
            bool erasure_sentinel = false;
            if ( Broodwar->isVisible( tile_pos ) ) {
                Unitset resource_tile = Broodwar->getUnitsOnTile( tile_pos, IsMineralField || IsResourceContainer || IsRefinery );  // Confirm it is present.
                if ( resource_tile.empty() ) {
                    r = inventory.resource_positions_.erase( r ); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
                    erasure_sentinel = true;
                }
            }

            if ( !erasure_sentinel ) {
                r++;
            }
        }

        //Update important variables.  Enemy stock has a lot of dependencies, updated above.
        inventory.updateLn_Army_Stock(friendly_inventory);
        inventory.updateLn_Tech_Stock(friendly_inventory);
        inventory.updateLn_Worker_Stock();
        inventory.updateVision_Count();

        inventory.updateLn_Supply_Remain(friendly_inventory);
        inventory.updateLn_Supply_Total();

        inventory.updateLn_Gas_Total();
        inventory.updateLn_Min_Total();

        inventory.updateGas_Workers();
        inventory.updateMin_Workers();

        inventory.updateMin_Possessed();
        inventory.updateHatcheries(friendly_inventory);  // macro variables, not every unit I have.

        inventory.updateReserveSystem();
        inventory.updateNextExpo(enemy_inventory, friendly_inventory);

        // Game time;
        int t_game = Broodwar->getFrameCount(); // still need this for mining script.
        buildorder.updateBuildingTimer(friendly_inventory);

      //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
        int map_x = Broodwar->mapWidth();
        int map_y = Broodwar->mapHeight();
        int map_area = map_x * map_y; // map area in tiles.

        //Knee-jerk states: gas, supply.
        gas_starved = (inventory.getLn_Gas_Ratio() < delta && Gas_Outlet()) || 
            (!buildorder.building_gene_.empty() && ( buildorder.building_gene_.begin()->getUnit().gasPrice() > Broodwar->self()->gas() || buildorder.building_gene_.begin()->getUpgrade().gasPrice() > Broodwar->self()->gas() ) );
        supply_starved = ( inventory.getLn_Supply_Ratio()  < gamma &&   //If your supply is disproportionately low, then you are gas starved, unless
                         Broodwar->self()->supplyTotal() <= 400 ); // you have not hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

        //Discontinuities (Cutoff if critically full, or suddenly progress towards one macro goal or another is impossible. 
        bool econ_possible = ((inventory.min_workers_ <= inventory.min_fields_ * 1.75 || inventory.min_fields_ < 36) && ( Count_Units(UnitTypes::Zerg_Drone, friendly_inventory) < 85)); // econ is only a possible problem if undersaturated or less than 62 patches, and worker count less than 90.
        bool vision_possible = true; // no vision cutoff ATM.
        bool army_possible = ( Broodwar->self()->supplyUsed() < 375 && exp(inventory.ln_army_stock_) / exp( inventory.ln_worker_stock_ ) < 2 * alpha_army / alpha_econ); // can't be army starved if you are maxed out (or close to it), Or if you have a wild K/L ratio.
        bool tech_possible = Tech_Avail(); // if you have no tech available, you cannot be tech starved.

        //Feed alpha values and cuttoff calculations into Cobb Douglas.
        CobbDouglas CD = CobbDouglas( alpha_army, exp(inventory.ln_army_stock_), army_possible, alpha_tech , exp(inventory.ln_tech_stock_), tech_possible, alpha_econ, exp(inventory.ln_worker_stock_), econ_possible );

        tech_starved = CD.tech_starved();
        army_starved = CD.army_starved();
        econ_starved = CD.econ_starved();
 
        double econ_derivative = CD.econ_derivative;
        double army_derivative = CD.army_derivative;
        double tech_derivative = CD.tech_derivative;

        //Unitset enemy_set = getEnemy_Set(enemy_inventory);
        enemy_inventory.updateUnitInventorySummary();
        friendly_inventory.updateUnitInventorySummary();
        inventory.est_enemy_stock_ = enemy_inventory.stock_total_ * (1 + 1 - inventory.vision_tile_count_ / (double)map_area); //assumes enemy stuff is uniformly distributed. Bad assumption.

        // Display the game status indicators at the top of the screen	
        if ( _ANALYSIS_MODE ) {

            Print_Unit_Inventory(0,50, friendly_inventory ); 
            Print_Upgrade_Inventory(375,70); 
            //Print_Unit_Inventory( 500, 170, enemy_inventory );
            Print_Build_Order_Remaining( 500, 170, buildorder );


            Broodwar->drawTextScreen(0, 0, "Reached Min Fields: %d", inventory.min_fields_ );
            Broodwar->drawTextScreen(0, 10, "Active Workers: %d", inventory.gas_workers_ + inventory.min_workers_ );
            Broodwar->drawTextScreen( 0, 20, "Active Miners: %d", inventory.min_workers_ );
            Broodwar->drawTextScreen( 0, 30, "Active Gas Miners: %d", inventory.gas_workers_ );

            Broodwar->drawTextScreen( 125, 0, "Econ Starved: %s", CD.econ_starved() ? "TRUE" : "FALSE" );  //
            Broodwar->drawTextScreen( 125, 10, "Army Starved: %s", CD.army_starved() ? "TRUE" : "FALSE" );  //
            Broodwar->drawTextScreen( 125, 20, "Tech Starved: %s", CD.tech_starved() ? "TRUE" : "FALSE" );  //

            Broodwar->drawTextScreen( 125, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE" );
            Broodwar->drawTextScreen( 125, 50, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE" );
            Broodwar->drawTextScreen( 125, 60, "Gas Outlet: %s", Gas_Outlet() ? "TRUE" : "FALSE" );  //

            Broodwar->drawTextScreen( 125, 80, "Ln Y/L: %4.2f", CD.getlny() ); //
            Broodwar->drawTextScreen( 125, 90, "Ln Y: %4.2f", CD.getlnY() ); //
            Broodwar->drawTextScreen( 125, 100, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60 ); //
            Broodwar->drawTextScreen( 125, 110, "Win Rate: %.2f", win_rate); //
            Broodwar->drawTextScreen( 125, 120, "Opponent: %s", Broodwar->enemy()->getRace().c_str() ); //

            Broodwar->drawTextScreen( 250, 0, "Econ Gradient: %.2g", CD.econ_derivative  );  //
            Broodwar->drawTextScreen( 250, 10, "Army Gradient: %.2g", CD.army_derivative ); //
            Broodwar->drawTextScreen( 250, 20, "Tech Gradient: %.2g", CD.tech_derivative ); //
            
            Broodwar->drawTextScreen( 250, 40, "Alpha_Econ: %4.2f %%", CD.alpha_econ * 100);  // As %s
            Broodwar->drawTextScreen( 250, 50, "Alpha_Army: %4.2f %%", CD.alpha_army * 100); //
            Broodwar->drawTextScreen( 250, 60, "Alpha_Tech: %4.2f %%", CD.alpha_tech * 100); //

            Broodwar->drawTextScreen( 250, 80, "Delta_gas: %4.2f", delta ); //
            Broodwar->drawTextScreen( 250, 90, "Gamma_supply: %4.2f", gamma ); //
            Broodwar->drawTextScreen( 250, 100, "Time to Completion: %d", buildorder.building_timer_ ); //
            Broodwar->drawTextScreen( 250, 110, "Freestyling: %s", buildorder.checkEmptyBuildOrder() && !buildorder.active_builders_ ? "TRUE" : "FALSE" ); //
            Broodwar->drawTextScreen( 250, 120, "Last Building: %s", buildorder.last_build_order.c_str() ); //

            //vision belongs here.

            Broodwar->drawTextScreen( 375, 20, "Enemy Stock(Est.): %d", inventory.est_enemy_stock_ );
            Broodwar->drawTextScreen( 375, 30, "Army Stock: %d", (int)exp(inventory.ln_army_stock_) ); //
            Broodwar->drawTextScreen( 375, 40, "Gas (Pct. Ln.): %4.2f", inventory.getLn_Gas_Ratio() );
            Broodwar->drawTextScreen( 375, 50, "Vision (Pct.): %4.2f",  inventory.vision_tile_count_ / (double)map_area );  //
            //Broodwar->drawTextScreen( 500, 130, "Supply Heuristic: %4.2f", inventory.getLn_Supply_Ratio() );  //
            //Broodwar->drawTextScreen( 500, 140, "Vision Tile Count: %d",  inventory.vision_tile_count_ );  //
            //Broodwar->drawTextScreen( 500, 150, "Map Area: %d", map_area );  //

            Broodwar->drawTextScreen( 500, 20, "Performance:" );  // 
            Broodwar->drawTextScreen( 500, 30, "APM: %d", Broodwar->getAPM() );  // 
            Broodwar->drawTextScreen( 500, 40, "APF: %4.2f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS() );  // 
            Broodwar->drawTextScreen( 500, 50, "FPS: %4.2f", Broodwar->getAverageFPS() );  // 
            Broodwar->drawTextScreen( 500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames() );  //

            for ( vector<int>::size_type p = 0; p != inventory.resource_positions_.size() ; ++p){
                if ( inventory.resource_positions_[p] ) {
                    if ( isOnScreen( inventory.resource_positions_[p] ) ) {
                        Broodwar->drawCircleMap( inventory.resource_positions_[p], (UnitTypes::Resource_Mineral_Field.dimensionUp() + UnitTypes::Resource_Mineral_Field.dimensionLeft()) / 2, Colors::Cyan ); // Plot their last known position.
                    }
                }
            }

            for ( vector<int>::size_type i = 0; i != inventory.buildable_positions_.size(); ++i ) {
                for ( vector<int>::size_type j = 0; j != inventory.buildable_positions_[i].size(); ++j ) {
                    if ( inventory.buildable_positions_[i][j] == false ) {
                        if ( isOnScreen( { (int)i * 32 + 16, (int)j * 32 + 16 } ) ) {
                            Broodwar->drawCircleMap( i * 32 + 16, j * 32 + 16, 1, Colors::Yellow );
                        }
                    }
                }
            } // both of these structures are on the same tile system.

            //for ( vector<int>::size_type i = 0; i != inventory.base_values_.size(); ++i ) {
            //    for ( vector<int>::size_type j = 0; j != inventory.base_values_[i].size(); ++j ) {
            //        if ( inventory.base_values_[i][j] > 1 ) {
            //            Broodwar->drawTextMap( i * 32 + 16, j * 32 + 16, "%d", inventory.base_values_[i][j] );
            //        }
            //    };
            //} // not that pretty to look at.

            for ( vector<int>::size_type i = 0; i < inventory.smoothed_barriers_.size(); ++i ) {
                for ( vector<int>::size_type j = 0; j < inventory.smoothed_barriers_[i].size(); ++j ) {
                    if ( inventory.smoothed_barriers_[i][j] == 0 ) {
                        if ( isOnScreen( { (int)i * 8 + 8, (int)j * 8 + 4 } ) ) {
                            //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.smoothed_barriers_[i][j] );
                            //Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
                        }
                    }
                    else if ( inventory.smoothed_barriers_[i][j] > 0 ){
                        if ( isOnScreen( { (int)i * 8 + 8, (int)j * 8 + 4 } ) ) {
                            //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.smoothed_barriers_[i][j] );
                            Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
                        }
                    }

                };
            } // Pretty to look at!

        }// close analysis mode

        auto end_preamble = std::chrono::high_resolution_clock::now();
        preamble_time = end_preamble - start_preamble;

     // Prevent spamming by only running our onFrame once every number of latency frames.
     // Latency frames are the number of frames before commands are processed.
        if ( Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0 )
            return;

        // Iterate through all the units that we own
        for ( auto &u : Broodwar->self()->getUnits() )
        {
            if ( _ANALYSIS_MODE ) {
                Broodwar->drawTextMap( u->getPosition(), u->getLastCommand().getType().c_str() );
            }
            // Ignore the unit if it no longer exists
            // Make sure to include this block when handling any Unit pointer!
            if ( !u || !u->exists() )
                continue;
            // Ignore the unit if it has one of the following status ailments
            if ( u->isLockedDown() ||
                u->isMaelstrommed() ||
                u->isStasised() )
                continue;
            // Ignore the unit if it is in one of the following states
            if ( u->isLoaded() ||
                !u->isPowered() ||
                u->isStuck() )
                continue;
            // Ignore the unit if it is incomplete or busy constructing
            if ( !u->isCompleted() ||
                u->isConstructing() )
                continue;

            // Finally make the unit do some stuff!
// Unit creation & Hatchery management loop
            auto start_larva = std::chrono::high_resolution_clock::now();
            if ( u->getType() == UnitTypes::Zerg_Larva ) // A resource depot is a Command Center, Nexus, or Hatchery.
            {
                // Build appropriate units. Check for suppply block, rudimentary checks for enemy composition.
                Reactive_Build( u, inventory, friendly_inventory, enemy_inventory );
            }
            auto end_larva = std::chrono::high_resolution_clock::now();

            // Worker Loop
            auto start_worker = std::chrono::high_resolution_clock::now();
            if ( u->getType().isWorker() )
            {
                // Mining loop if our worker is idle (includes returning $$$) or not moving while gathering gas, we (re-) evaluate what they should be mining.  Original script uses isIdle() only. might have queues that are very long which is why they may be unresponsive.
                if ( (isIdleEmpty( u ) || u->isGatheringMinerals() || u->isGatheringGas() || u->isCarryingGas() || u->isCarryingMinerals()) && t_game % 75 == 0 )
                {
                    // Order workers carrying a resource to return them to the center, every few seconds. This will refresh their logics as well.
                    // otherwise find a mineral patch to harvest.
                    if ( !u->isCarryingGas() && !u->isCarryingMinerals() )
                    {
                        // Idle worker then Harvest from the nearest mineral patch or gas refinery, depending on need.
                        bool enough_gas = !gas_starved ||
                            (Count_Units( UnitTypes::Zerg_Extractor, friendly_inventory ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Extractor )) == 0 ||
                            inventory.gas_workers_ >= 3 * Count_Units( UnitTypes::Zerg_Extractor, friendly_inventory );  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.  Does not count worker IN extractor.

                        bool excess_minerals = inventory.min_workers_ >= 1 * inventory.min_fields_; //Some extra leeway over the optimal 1.5/patch, since they will be useless overgathering gas but not useless overgathering minerals.

                        if ( !enough_gas /*&& excess_minerals*/ ) // Careful tinkering here.
                        {
                            Unit ref = u->getClosestUnit( IsRefinery && IsOwned );
                            if ( ref && ref->exists() ) {
                                Worker_Gas( u );
                                ++inventory.gas_workers_;
                            }
                        } // closure gas
                        else if ( !excess_minerals || enough_gas ) // pull from gas if we are satisfied with our gas count.
                        {
                            Worker_Mine( u );
                            ++inventory.min_fields_;
                        }

                    } // closure: collection assignment.
                    else if ( u->isCarryingMinerals() || u->isCarryingGas() ) // Return $$$
                    {
                        Unit base = u->getClosestUnit( IsResourceDepot && IsOwned );
                        if ( base && base->exists() ) {
                            u->move( base->getPosition() );
                        }
                        u->returnCargo( true );
                    }//Closure: returning $$ loop
                }// Closure: mining loop

// Building subloop. Resets every few frames.
                if ( (isIdleEmpty( u ) /*|| IsGatheringMinerals( u ) || IsGatheringGas( u )*/) )
                { //only get those that are idle or gathering minerals, but not carrying them. This always irked me. 

                    //t_build = Broodwar->getFrameCount();
                    Building_Begin( u, inventory, enemy_inventory );

                } // Close Build loop
            } // Close Worker management loop
            auto end_worker = std::chrono::high_resolution_clock::now();

            //Scouting/vision loop. Intially just brownian motion, now a fully implemented boids-type algorithm.
            auto start_scout = std::chrono::high_resolution_clock::now();
            if ( isIdleEmpty( u ) && !u->isAttacking() && !u->isUnderAttack() && u->getType() != UnitTypes::Zerg_Drone &&  u->getType() != UnitTypes::Zerg_Larva && u->canMove() )
            { //Scout if you're not a drone or larva and can move.
                Boids boids;
                if ( u->getType() == UnitTypes::Zerg_Overlord ) {
                    boids.Boids_Movement( u, 2, friendly_inventory, enemy_inventory, inventory ); // keep this because otherwise they clump up very heavily, like mutas. Don't want to lose every overlord to one AOE.
                }
                else {
                    boids.Boids_Movement( u, 0, friendly_inventory, enemy_inventory, inventory );
                }
            } // If it is a combat unit, then use it to attack the enemy.
            auto end_scout = std::chrono::high_resolution_clock::now();

            //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground. 
            auto start_combat = std::chrono::high_resolution_clock::now();
            if ( u->getType() != UnitTypes::Zerg_Larva && u->getType().canAttack() )
            {
                Unit_Inventory enemy_loc = getUnitInventoryInRadius( enemy_inventory, u->getPosition(), 1024 ); //automatically populates with stock values from cache calculated Onframe, don't need to recalculate them.
                if ( !enemy_loc.unit_inventory_.empty() ) { // if there are bad guys nearby, continue search for friends nearby.
                    Boids boids;
                    if ( army_derivative != 0 || u->getType() == UnitTypes::Zerg_Drone) { //In normal, non-massive army scenarioes...

                        Unit_Inventory friend_loc = getUnitInventoryInRadius( friendly_inventory, u->getPosition(), enemy_loc.max_range_ );

                        if ( !friend_loc.unit_inventory_.empty() ) { // if there is enemy and you exist (implied by friends).

                            friend_loc.updateUnitInventorySummary();
                            enemy_loc.updateUnitInventorySummary();

                            int enemy_stock_loc = enemy_loc.stock_fliers_ + enemy_loc.stock_ground_units_; //Consider combat when there is at least a single enemy within a hefty range.

                            //Tally up crucial details about enemy. 
                            int e_count = enemy_loc.unit_inventory_.size();

                            int helpless_e = u->isFlying() ? enemy_loc.stock_cannot_shoot_up_ : enemy_loc.stock_cannot_shoot_down_ ;
                            int helpful_e = u->isFlying() ? enemy_loc.stock_cannot_shoot_down_ : enemy_loc.stock_cannot_shoot_up_;
                            //int helpless_u = 0; // filled below.  Need to actually reflect MY inventory.
                            int helpful_u = 0; // filled below.

                            if ( enemy_inventory.stock_fliers_ > 0 ) {
                                helpful_u += Stock_Units_ShootUp( friend_loc ); // double-counts hydras.
                            } 
                            if ( enemy_inventory.stock_ground_units_ > 0 ) {
                                helpful_u += Stock_Units_ShootDown( friend_loc );
                            }

                            //if ( u->getType().airWeapon() != WeaponTypes::None ) {
                            //    helpless_u += Stock_Units_ShootDown( friend_loc );
                            //}
                            //if ( u->getType().groundWeapon() != WeaponTypes::None ) {
                            //    helpless_u += enemy_loc.stock_ground_units_;
                            //}

                            int friend_stock_loc = helpful_u ; // Assess your comparative value.

                            int f_drone = 0;

                            int dist = enemy_loc.max_range_;

                            Unit e_closest = u->getClosestUnit( IsEnemy, dist ); // Defining this unit to be closest enemy below.

                            for ( auto e = enemy_loc.unit_inventory_.begin(); e != enemy_loc.unit_inventory_.end(); e++ ) { // trims the set for each attackable enemy. We move towards the center of these units
                                if ( u->getDistance( (*e).second.pos_ ) <= dist ) {
                                    e_closest = (*e).second.bwapi_unit_;
                                    dist = u->getDistance( (*e).second.pos_ );
                                }
                            }

                            for ( auto f = friend_loc.unit_inventory_.begin(); f != friend_loc.unit_inventory_.end(); f++ ) { // trims the set for each attackable enemy. We move towards the center of these units
                                if ( (*f).second.type_ == UnitTypes::Zerg_Drone ) {
                                    f_drone++;
                                }
                            }

                            if ( e_closest && e_closest->exists() ) {

                                bool retreat = u->canMove() && ( // one of the following conditions are true:
                                    army_starved || // Run if you are missing troops.
                                    //helpful_u == 0 || // Run if you contribute nothing.
                                    !e_closest->isDetected() || // Run if they are cloaked.
                                    helpful_e > 1.25 * helpful_u || // Run if they have local advantage on you
                                    u->isUnderStorm() || u->isUnderDisruptionWeb() || u->isUnderDarkSwarm() || u->isIrradiated() || // Run if spelled.
                                    ( u->getType().isFlyer() && helpful_e > 0.25 * friend_loc.stock_fliers_ ) || //  Run if fliers face more than token resistance.
                                    ( u->getHitPoints() < 0.25 * u->getType().maxHitPoints() && u->isFlying() ) || // Run if you are crippled and fly
                                    //( e_closest->isInWeaponRange( u ) && ( u->getType().airWeapon().maxRange() > e_closest->getType().airWeapon().maxRange() || u->getType().groundWeapon().maxRange() > e_closest->getType().groundWeapon().maxRange() ) ) || // If you outrange them and they are attacking you. Kiting.
                                    ( u->getType() == UnitTypes::Zerg_Drone && (!army_starved || u->getHitPoints() < 0.50 * u->getType().maxHitPoints() ) ) // Run if drone and (we have forces elsewhere or the drone is injured).
                                    );

                                bool force_attack = (f_drone > 0 && u->getType() != UnitTypes::Zerg_Drone) || //Don't run if drones are present.
                                    u->isInWeaponRange( e_closest ) && u->getType().airWeapon().maxRange() == 0 && u->getType().groundWeapon().maxRange() == 0; // don't run if they're in range and you're melee.

                                bool sufficient_attack = helpful_e < 0.75 * helpful_u || // Don't run if you outclass them and your boys are ready to fight.
                                    inventory.est_enemy_stock_ < 0.75 * exp( inventory.ln_army_stock_ ); // you have a global advantage.

                                if ( sufficient_attack || !(retreat && !force_attack) ) {

                                    boids.Tactical_Logic( u, enemy_loc, Colors::Orange ); // move towards enemy untill tactical logic takes hold at about 150 range.

                                    if ( _ANALYSIS_MODE ) {
                                        if ( isOnScreen( u->getPosition() ) ) {
                                            Broodwar->drawTextMap( u->getPosition().x, u->getPosition().y, "%d", friend_stock_loc, Colors::Green );
                                        }
                                        if ( isOnScreen( enemy_loc.getMeanLocation() ) ) {
                                            Broodwar->drawTextMap( enemy_loc.getMeanLocation().x, enemy_loc.getMeanLocation().y, "%d", enemy_loc.stock_ground_units_ + enemy_loc.stock_fliers_, Colors::Red );
                                        }
                                    }
                                }
                                else  {
                                    boids.Retreat_Logic( u, e_closest, friendly_inventory, inventory, Colors::White );
                                }
                            }
                        } // close local examination.
                    }
                    else { // who cares what they have if the override is triggered?
                        boids.Tactical_Logic( u, enemy_set_all, Colors::Orange ); // enemy inventory?
                        //if ( _ANALYSIS_MODE ) {
                        //    Broodwar->drawTextMap( enemies_loc.getPosition().x, enemies_loc.getPosition().y, "%d", enemy_loc.stock_ground_units_ + enemy_loc.stock_fliers_, Colors::Red ); // this is improperly placed, but still useful.
                        //}
                    }
                }
            }
            auto end_combat = std::chrono::high_resolution_clock::now();

//Upgrade loop:
            auto start_upgrade = std::chrono::high_resolution_clock::now();
            if ( isIdleEmpty( u ) && !u->canAttack() && u->getType() != UnitTypes::Zerg_Larva && // no trying to morph hydras anymore.
                (u->canUpgrade() || u->canResearch() || u->canMorph())  ) { // this will need to be revaluated once I buy units that cost gas.

                Tech_Begin( u , friendly_inventory);

                //PrintError_Unit( u );
            }
            auto end_upgrade = std::chrono::high_resolution_clock::now();

//Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
            auto start_creepcolony = std::chrono::high_resolution_clock::now();
            if (  u->getType() == UnitTypes::Zerg_Creep_Colony && army_starved &&
                ( Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 || Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) > 0) ) {

                //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?
                int fliers = 0;
                int cannot_shoot_up = 0;
                int high_ground = 0;
                int e_count = 0;

                for ( auto e_iter = enemy_inventory.unit_inventory_.begin(); e_iter != enemy_inventory.unit_inventory_.end(); e_iter++ ) {
                    if ( e_iter->second.type_.isFlyer() ) {
                        fliers++;
                    }
                    if ( e_iter->second.type_.airWeapon() == WeaponTypes::None ) {
                        cannot_shoot_up++;
                    }
                    Region r = Broodwar->getRegionAt( e_iter->second.pos_ );
                    if ( e_iter->second.valid_pos_ ) {
                        if ( r->isHigherGround() || r->getDefensePriority() > 1 ) {
                            high_ground++;
                        }
                    }
                    e_count++;
                } // get closest enemy unit in inventory.

                if ( Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) > 0 ) {
                    if ( fliers > 0.15 * e_count ) { // if they have a flyer (that can attack), get spores.
                        Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, friendly_inventory, true );
                    }
                    else {
                        Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true );
                    }
                } // build one of the two colonies based on the presence of closest units.
                else if ( Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) == 0 ) {
                    Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true );
                } // build sunkens if you only have that
                else if ( Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) == 0 &&
                    Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) > 0 ) {
                    Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, friendly_inventory, true );
                } // build spores if you only have that.
            } // closure: Creep colony loop
            auto end_creepcolony = std::chrono::high_resolution_clock::now();

            larva_time += end_larva - start_larva;
            worker_time += end_worker - start_worker;
            scout_time += end_scout - start_scout;
            combat_time += end_combat - start_combat;
            upgrade_time += end_upgrade - start_upgrade;
            creepcolony_time += end_creepcolony - start_creepcolony;
        } // closure: unit iterator

        // Detectors are called for cloaked units. Only if you're not supply starved, because we only have overlords for detectors.
        auto start_detector = std::chrono::high_resolution_clock::now();
            Position c; // holder for cloaked unit position.
            bool sentinel_value = false;
            if ( !army_starved && !supply_starved ) {
                for ( auto e = enemy_inventory.unit_inventory_.begin(); e != enemy_inventory.unit_inventory_.end() && !enemy_inventory.unit_inventory_.empty(); e++ ) {
                    if ( (*e).second.type_.isCloakable() || (*e).second.type_ == UnitTypes::Zerg_Lurker || (*e).second.type_.hasPermanentCloak() || (*e).second.type_.isBurrowable() ) {
                        c = (*e).second.pos_; // then we have to send in some vision.
                        sentinel_value = true;
                    } //some units, DT, Observers, are not cloakable. They are cloaked though. Recall burrow and cloak are different.
                }
                if ( sentinel_value ) {
                    int dist = 999999;
                    int dist_temp = 0;
                    bool detector_found = false;
                    Unit detector_of_choice;
                    for ( auto d = Broodwar->self()->getUnits().begin(); d != Broodwar->self()->getUnits().end(); d++ ) {
                        if ( (*d)->getType() == UnitTypes::Zerg_Overlord && !(*d)->isUnderAttack() && (*d)->getHitPoints() > 0.25 * (*d)->getInitialHitPoints() ) {
                            dist_temp = (*d)->getDistance( c );
                            if ( dist_temp < dist ) {
                                dist = dist_temp;
                                detector_of_choice = (*d);
                                detector_found = true;
                            }
                        }
                    }
                    if ( detector_found ) {
                        detector_of_choice->move( c );
                        if ( _ANALYSIS_MODE ) {
                            Broodwar->drawCircleMap( c, 25, Colors::Cyan );
                            Diagnostic_Line( detector_of_choice->getPosition(), c, Colors::Cyan );
                        }
                    }
                }
            }
        auto end_detector = std::chrono::high_resolution_clock::now();

        detector_time += end_detector - start_detector;

        auto end = std::chrono::high_resolution_clock::now();
        total_frame_time = end - start_preamble;
        preamble_time;
        larva_time;
        worker_time;
        scout_time;
        combat_time;
        detector_time;
        upgrade_time;
        creepcolony_time;
        total_frame_time; //will use preamble start time.

        //Clock App
            if ( total_frame_time.count() > 55 ) {
                short_delay+=1;
            }
            if ( total_frame_time.count() > 1000 ) {
                med_delay+=1;
            }
            if ( total_frame_time.count() > 10000 ) {
                long_delay+=1;
            }
            if ( _ANALYSIS_MODE ) {
                Broodwar->drawTextScreen( 500, 70, "Delays:{S:%d,M:%d,L:%d}%3.fms", short_delay, med_delay, long_delay, total_frame_time.count() ); // Flickers. Annoying.
                Broodwar->drawTextScreen( 500, 80, "Preamble:%3.f %%", preamble_time.count()/ total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 90, "Larva:%3.f %%", larva_time / total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 100, "Workers:%3.f %%", worker_time / total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 110, "Scouting:%3.f %%", scout_time / total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 120, "Combat:%3.f %%", combat_time / total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 130, "Detection:%3.f %%", detector_time / total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 140, "Upgrades:%3.f %%", upgrade_time / total_frame_time.count() * 100 );
                Broodwar->drawTextScreen( 500, 150, "CreepColonies:%3.f %%", creepcolony_time / total_frame_time.count() * 100 );
                if ( (short_delay > 320 || Broodwar->elapsedTime() > 90 * 60) && _ANALYSIS_MODE == true ) //if game times out or lags out, end game with resignation.
                {
                    Broodwar->leaveGame();
                }
            }

} // closure: Onframe

void MeatAIModule::onSendText( std::string text )
{

    // Send the text to the game if it is not being processed.
    Broodwar->sendText( "%s", text.c_str() );

    // Make sure to use %s and pass the text as a parameter,
    // otherwise you may run into problems when you use the %(percent) character!

}

void MeatAIModule::onReceiveText( BWAPI::Player player, std::string text )
{
    // Parse the received text
    Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void MeatAIModule::onPlayerLeft( BWAPI::Player player )
{
    // Interact verbally with the other players in the game by
    // announcing that the other player has left.
    Broodwar->sendText( "Goodbye %s!", player->getName().c_str() );
}

void MeatAIModule::onNukeDetect( BWAPI::Position target )
{

    // Check if the target is a valid position
    if ( target )
    {
        // if so, print the location of the nuclear strike target
        Broodwar << "Nuclear Launch Detected at " << target << std::endl;
    }
    else
    {
        // Otherwise, ask other players where the nuke is!
        Broodwar->sendText( "Where's the nuke?" );
    }

    // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void MeatAIModule::onUnitDiscover( BWAPI::Unit unit )
{
    if ( unit && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check.
        //Broodwar->sendText( "I just gained vision of a %s", unit->getType().c_str() );
        Stored_Unit eu = Stored_Unit( unit );

        if ( enemy_inventory.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
            //Broodwar->sendText( "A %s just was discovered. Added to unit inventory, size %d", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else { // the insertion must have failed
            //Broodwar->sendText( "%s is already at address %p.", eu.type_.c_str(), enemy_inventory.unit_inventory_.find( unit ) ) ;
        }
    }
}

void MeatAIModule::onUnitEvade( BWAPI::Unit unit )
{
    //if ( unit && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check.
    //                                                                //Broodwar->sendText( "I just gained vision of a %s", unit->getType().c_str() );
    //    Stored_Unit eu = Stored_Unit( unit );

    //    if ( enemy_inventory.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
    //        Broodwar->sendText( "A %s just evaded me. Added to hiddent unit inventory, size %d", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
    //    }
    //    else { // the insertion must have failed
    //        Broodwar->sendText( "Insertion of %s failed.", eu.type_.c_str() );
    //    }
    //}
}

void MeatAIModule::onUnitShow( BWAPI::Unit unit )
{
    //if ( unit && unit->exists() && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.. (old comment?)
    //    Stored_Unit eu = Stored_Unit( unit );
    //    auto found_ptr = enemy_inventory.unit_inventory_.find( unit );
    //    if ( found_ptr != enemy_inventory.unit_inventory_.end() ) {
    //        enemy_inventory.unit_inventory_.erase( unit );
    //        Broodwar->sendText( "Redscovered a %s, hidden unit inventory is now %d.", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
    //    }
    //    else {
    //        Broodwar->sendText( "Discovered a %s.", unit->getType().c_str() );
    //    }
    //}
}

void MeatAIModule::onUnitHide( BWAPI::Unit unit )
{


}

void MeatAIModule::onUnitCreate( BWAPI::Unit unit )
{
    if ( Broodwar->isReplay() )
    {
        // if we are in a replay, then we will print out the build order of the structures
        if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText( "%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }
}

void MeatAIModule::onUnitDestroy( BWAPI::Unit unit )
{
    if ( unit && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        Stored_Unit eu = Stored_Unit( unit );
        auto found_ptr = enemy_inventory.unit_inventory_.find( unit );
        if ( found_ptr != enemy_inventory.unit_inventory_.end() ) {
            enemy_inventory.unit_inventory_.erase( unit );
            //Broodwar->sendText( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else {
            //Broodwar->sendText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_inventory.unit_inventory_.size() );
        }
    }
}

void MeatAIModule::onUnitMorph( BWAPI::Unit unit )
{
    if ( Broodwar->isReplay() )
    {
        // if we are in a replay, then we will print out the build order of the structures
        if ( unit->getType().isBuilding() &&
            !unit->getPlayer()->isNeutral() )
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText( "%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }

    buildorder.updateRemainingBuildOrder( unit );

}

void MeatAIModule::onUnitRenegade( BWAPI::Unit unit )
{
}

void MeatAIModule::onSaveGame( std::string gameName )
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void MeatAIModule::onUnitComplete( BWAPI::Unit unit )
{
}