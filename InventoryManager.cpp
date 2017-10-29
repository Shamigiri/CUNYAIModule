#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"


using namespace std;

// Creates a Inventory Object
Inventory::Inventory() {};
Inventory::Inventory( const Unit_Inventory &ui, const Resource_Inventory &ri ) {

    updateLn_Army_Stock( ui );
    updateLn_Tech_Stock( ui );
    updateLn_Worker_Stock();
    updateVision_Count();

    updateLn_Supply_Remain( ui );
    updateLn_Supply_Total();

    updateLn_Gas_Total();
    updateLn_Min_Total();

    updateGas_Workers();
    updateMin_Workers();

    updateMin_Possessed();
    updateHatcheries( ui );

    updateReserveSystem();
		
    if ( smoothed_barriers_.size() == 0 ) {
        updateSmoothPos();
        int unwalkable_ct = 0;
        for ( vector<int>::size_type i = 0; i != smoothed_barriers_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != smoothed_barriers_[i].size(); ++j ) {
                unwalkable_ct += smoothed_barriers_[i][j];
            }
        }
        Broodwar->sendText( "There are %d tiles, and %d smoothed out tiles.", smoothed_barriers_.size(), unwalkable_ct );
    }

	if (ri.resource_inventory_.size() == 0) {
		updateBuildablePos();
		updateBaseLoc( ri );
		int buildable_ct = 0;
		for (vector<int>::size_type i = 0; i != buildable_positions_.size(); ++i) {
			for (vector<int>::size_type j = 0; j != buildable_positions_[i].size(); ++j) {
				buildable_ct += buildable_positions_[i][j];
			}
		}
		Broodwar->sendText("There are %d resources on the map, %d canidate expo positions.", ri.resource_inventory_.size(), buildable_ct);
	}

    if ( map_veins_.size() == 0 ) {
        updateMapVeins();
        int vein_ct = 0;
        for ( vector<int>::size_type i = 0; i != map_veins_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j != map_veins_[i].size(); ++j ) {
                if ( map_veins_[i][j] > 10 ) {
                    ++vein_ct;
                }
            }
        }
        Broodwar->sendText( "There are %d roughly tiles, %d veins.", map_veins_.size(), vein_ct );
    }

	if (start_positions_.empty() && !list_cleared_) {
		getStartPositions();
	}
};

// Defines the (safe) log of our army stock.
void Inventory::updateLn_Army_Stock(const Unit_Inventory &ui) {

    double total = ui.stock_total_;
    for ( auto & u : ui.unit_inventory_ ) {
        if ( u.second.type_ == UnitTypes::Zerg_Drone ) {
            total -= u.second.current_stock_value_;
        }
    }

    if ( total <= 0 ) {
        total = 1;
    }

    ln_army_stock_ = log( total );
};

// Updates the (safe) log of our tech stock.
void Inventory::updateLn_Tech_Stock( const Unit_Inventory &ui ) {

    double total = 0;

    for ( int i = 132; i != 143; i++ )
    { // iterating through all tech buildings. See enumeration of unittype for details.
        UnitType build_current = (UnitType)i;
        total += MeatAIModule::Stock_Buildings( build_current, ui );
    }

    for ( int i = 0; i != 62; i++ )
    { // iterating through all upgrades.
        UpgradeType up_current = (UpgradeType)i;
        total += MeatAIModule::Stock_Ups( up_current );
    }

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_tech_stock_ = log( total );
};

// Updates the (safe) log of our worker stock. calls both worker updates to be safe.
void Inventory::updateLn_Worker_Stock() {

    double total = 0;

    double cost = sqrt( pow( UnitTypes::Zerg_Drone.mineralPrice(), 2 ) + pow( 1.25 * UnitTypes::Zerg_Drone.gasPrice(), 2 ) + pow( 25 * UnitTypes::Zerg_Drone.supplyRequired(), 2 ) );

    updateGas_Workers();
    updateMin_Workers();

    int workers = gas_workers_ + min_workers_;

    total = cost * workers;

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_worker_stock_ = log( total );
};

// Updates the (safe) log of our supply stock. Looks specifically at our morphing units as "available".
void Inventory::updateLn_Supply_Remain( const Unit_Inventory &ui ) {

    double total = 0;
    for ( int i = 37; i != 48; i++ )
    { // iterating through all units.  (including buildings).
        UnitType u_current = (UnitType)i;
        total += MeatAIModule::Stock_Supply( u_current, ui );
    }

    total = total - Broodwar->self()->supplyUsed();

    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_remain_ = log( total );
};

// Updates the (safe) log of our consumed supply total.
void Inventory::updateLn_Supply_Total() {

    double total = Broodwar->self()->supplyTotal();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_supply_total_ = log( total );
};

// Updates the (safe) log of our gas total.
void Inventory::updateLn_Gas_Total() {

    double total = Broodwar->self()->gas();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_gas_total_ = log( total );
};

// Updates the (safe) log of our mineral total.
void Inventory::updateLn_Min_Total() {

    double total = Broodwar->self()->minerals();
    if ( total <= 0 ) {
        total = 1;
    } // no log 0 under my watch!.

    ln_min_total_ = log( total );
};

// Updates the (safe) log of our gas total. Returns very high int instead of infinity.
double Inventory::getLn_Gas_Ratio() {
    // Normally:
    if ( ln_min_total_ > 0 || ln_gas_total_ > 0 ) {
        return ln_gas_total_ / (ln_min_total_ + ln_gas_total_);
    }
    else {
        return 99999;
    } // in the alternative case, you have nothing - you're mineral starved, you need minerals, not gas. Define as ~~infty, not 0.
};

// Updates the (safe) log of our supply total. Returns very high int instead of infinity.
double Inventory::getLn_Supply_Ratio() {
    // Normally:
    if ( ln_supply_total_ > 0 ) {
        return ln_supply_remain_ / ln_supply_total_ ;
    }
    else {
        return 99999;
    } // in the alternative case, you have nothing - you're supply starved. Probably dead, too. Just in case- Define as ~~infty, not 0.
};

// Updates the count of our gas workers.
void Inventory::updateGas_Workers() {
    // Get worker tallies.
    int gas_workers = 0;

    Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
    if ( !myUnits.empty() ) { // make sure this object is valid!
        for ( auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u )
        {
            if ( (*u) && (*u)->exists() ) {
                if ( (*u)->getType().isWorker() ) {
                    if ( (*u)->isGatheringGas() || (*u)->isCarryingGas() ) // implies exists and isCompleted
                    {
                        ++gas_workers;
                    }
                } // closure: Only investigate closely if they are drones.
            } // Closure: only investigate on existance of unit..
        } // closure: count all workers
    }

    gas_workers_ = gas_workers;
}

// Updates the count of our mineral workers.
void Inventory::updateMin_Workers() {
    // Get worker tallies.
    int min_workers = 0;

    Unitset myUnits = Broodwar->self()->getUnits(); // out of my units  Probably easier than searching the map for them.
    if ( !myUnits.empty() ) { // make sure this object is valid!
        for ( auto u = myUnits.begin(); u != myUnits.end() && !myUnits.empty(); ++u )
        {
            if ( (*u) && (*u)->exists() ) {
                if ( (*u)->getType().isWorker() ) {
                    if ( (*u)->isGatheringMinerals() || (*u)->isCarryingMinerals() ) // implies exists and isCompleted
                    {
                        ++min_workers;
                    }
                } // closure: Only investigate closely if they are drones.
            } // Closure: only investigate on existance of unit..
        } // closure: count all workers
    }

    min_workers_ = min_workers;
}

// Updates the number of mineral fields we "possess".
void Inventory::updateMin_Possessed() {

    int min_fields = 0;
    Unitset resource = Broodwar->getMinerals(); // get any mineral field that exists on the map.
    if ( !resource.empty() ) { // check if the minerals exist
        for ( auto r = resource.begin(); r != resource.end() && !resource.empty(); ++r ) { //for each mineral
            if ( (*r) && (*r)->exists() ) {
                Unitset mybases = Broodwar->getUnitsInRadius( (*r)->getPosition(), 250, Filter::IsResourceDepot && Filter::IsOwned ); // is there a mining base near there?
                if ( !mybases.empty() ) { // check if there is a base nearby
                    min_fields++; // if there is a base near it, then this mineral counts.
                } // closure if base is nearby
            } // closure for existance check.
        } // closure: for each mineral
    } // closure, minerals are visible on map.

    min_fields_ = min_fields;
}

// Updates the count of our vision total, in tiles
void Inventory::updateVision_Count() {
    int map_x = BWAPI::Broodwar->mapWidth();
    int map_y = BWAPI::Broodwar->mapHeight();

    int map_area = map_x * map_y; // map area in tiles.
    int total_tiles = 0; 
    for ( int tile_x = 1; tile_x <= map_x; tile_x++ ) { // there is no tile (0,0)
        for ( int tile_y = 1; tile_y <= map_y; tile_y++ ) {
            if ( BWAPI::Broodwar->isVisible( tile_x, tile_y ) ) {
                total_tiles += 1;
            }
        }
    } // this search must be very exhaustive to do every frame. But C++ does it without any problems.

    if ( total_tiles == 0 ) {
        total_tiles = 1;
    } // catch some odd case where you are dead anyway. Rather not crash.
    vision_tile_count_ = total_tiles;
}

// Updates the number of hatcheries (and decendent buildings).
void Inventory::updateHatcheries( const Unit_Inventory &ui ) {
    hatches_ = MeatAIModule::Count_Units( UnitTypes::Zerg_Hatchery, ui ) +
               MeatAIModule::Count_Units( UnitTypes::Zerg_Lair, ui ) +
               MeatAIModule::Count_Units( UnitTypes::Zerg_Hive, ui );
}


//In Tiles?
void Inventory::updateBuildablePos()
{
    //Buildable_positions_ = std::vector< std::vector<bool> >( BWAPI::Broodwar->mapWidth()/8, std::vector<bool>( BWAPI::Broodwar->mapHeight()/8, false ) );

    int map_x = Broodwar->mapWidth() ;
    int map_y = Broodwar->mapHeight() ;
    for ( int x = 0; x <= map_x; ++x ) {
        vector<bool> temp;
        for ( int y = 0; y <= map_y; ++y ) {
             temp.push_back( Broodwar->isBuildable( x, y ) );
        }
        buildable_positions_.push_back( temp );
    }
};

void Inventory::updateSmoothPos() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4 ; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    int choke_score = 0;

    // first, define matrixes to recieve the walkable locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( !Broodwar->isWalkable( x , y ) ); 
        }
        smoothed_barriers_.push_back( temp );
    }

    for ( auto iter = 2; iter < 100; iter++ ) { // iteration 1 is already done by labling unwalkables.
        for ( auto minitile_x = 1; minitile_x <= map_x ; ++minitile_x ) {
            for ( auto minitile_y = 1; minitile_y <= map_y ; ++minitile_y ) { // Check all possible walkable locations.

                // Psudocode: if any two opposing points are unwalkable, or the corners are blocked off, while an alternative path through the center is walkable, it can be smoothed out, the fewer cycles it takes to identify this, the rougher the surface.
                // Repeat untill finished.

                if ( smoothed_barriers_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                    // Predefine grid we will search over.
                    bool local_tile_0_0 = (smoothed_barriers_[(minitile_x - 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x - 1)][(minitile_y - 1)] > 0) ;
                    bool local_tile_1_0 = (smoothed_barriers_[minitile_x][(minitile_y - 1)]       < iter && smoothed_barriers_[minitile_x][(minitile_y - 1)] > 0 )      ;
                    bool local_tile_2_0 = (smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] < iter && smoothed_barriers_[(minitile_x + 1)][(minitile_y - 1)] > 0) ;

                    bool local_tile_0_1 = (smoothed_barriers_[(minitile_x - 1)][minitile_y] < iter  && smoothed_barriers_[(minitile_x - 1)][minitile_y] > 0) ;
                    bool local_tile_1_1 = (smoothed_barriers_[minitile_x][minitile_y]       < iter  && smoothed_barriers_[minitile_x][minitile_y] > 0)       ;
                    bool local_tile_2_1 = (smoothed_barriers_[(minitile_x + 1)][minitile_y] < iter  && smoothed_barriers_[(minitile_x + 1)][minitile_y]> 0)  ;

                    bool local_tile_0_2 = (smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)]  < iter  && smoothed_barriers_[(minitile_x - 1)][(minitile_y + 1)] > 0)  ;
                    bool local_tile_1_2 = (smoothed_barriers_[minitile_x][(minitile_y + 1)]        < iter  && smoothed_barriers_[minitile_x][(minitile_y + 1)] > 0)        ;
                    bool local_tile_2_2 = (smoothed_barriers_[(minitile_x + 1)][(minitile_y + 1)]  < iter  && smoothed_barriers_[(minitile_x + 1)][(minitile_y + 1)] > 0)  ;

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_tile_0_0 && (local_tile_2_2 || local_tile_2_1 || local_tile_1_2)) ||
                        (local_tile_1_0 && (local_tile_1_2 || local_tile_0_2 || local_tile_2_2)) ||
                        (local_tile_2_0 && (local_tile_0_2 || local_tile_0_1 || local_tile_1_2)) ||
                        (local_tile_0_1 && (local_tile_2_1 || local_tile_2_0 || local_tile_2_2));

                    bool open_path =
                        (!local_tile_0_0 && (!local_tile_2_2 || !local_tile_2_1 || !local_tile_1_2)) ||
                        (!local_tile_1_0 && (!local_tile_1_2 || !local_tile_0_2 || !local_tile_2_2)) ||
                        (!local_tile_2_0 && (!local_tile_0_2 || !local_tile_0_1 || !local_tile_1_2)) ||
                        (!local_tile_0_1 && (!local_tile_2_1 || !local_tile_2_0 || !local_tile_2_2));

                    if ( open_path && ( opposing_tiles ) ) { // if it is closing off, but still has open space, mark as special and continue.  Will prevent algorithm from sealing map.
                        smoothed_barriers_[minitile_x][minitile_y] = 99 - iter;
                    }

                    if ( !open_path && (opposing_tiles ) ) { // if it is closed off or blocked, then seal it up and continue. 
                        smoothed_barriers_[minitile_x][minitile_y] = iter;
                    }
                    
                }
            }
        }
    }
}

void Inventory::updateMapVeins() {
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 

    // first, define matrixes to recieve the smoothed locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( smoothed_barriers_[x][y] > 0 );  //Was that location smoothed out?
        }
        map_veins_.push_back( temp );
    }

    for ( auto iter = 2; iter < 300; iter++ ) { // iteration 1 is already done by labling unwalkables.
        for ( auto minitile_x = 1; minitile_x <= map_x; ++minitile_x ) {
            for ( auto minitile_y = 1; minitile_y <= map_y; ++minitile_y ) { // Check all possible walkable locations.

                                                                 // Psudocode: if any two opposing points are unwalkable, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
                                                                 // If any 3 points adjacent are unwalkable it is probably just a bad place to walk, dead end, etc. Mark it as unwalkable.  Do not consider it unwalkable this cycle.
                                                                 // if any corner of it is inaccessable, it is a diagonal wall, mark it as unwalkable. Do not consider it unwalkable this cycle.
                                                                 // Repeat untill finished.

                if ( map_veins_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                    // Predefine grid we will search over.
                    bool local_tile_0_0 = (map_veins_[(minitile_x - 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y - 1)] > 0);
                    bool local_tile_1_0 = (map_veins_[minitile_x][(minitile_y - 1)]       < iter && map_veins_[minitile_x][(minitile_y - 1)] > 0);
                    bool local_tile_2_0 = (map_veins_[(minitile_x + 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y - 1)] > 0);

                    bool local_tile_0_1 = (map_veins_[(minitile_x - 1)][minitile_y] < iter  && map_veins_[(minitile_x - 1)][minitile_y] > 0);
                    bool local_tile_1_1 = (map_veins_[minitile_x][minitile_y]       < iter  && map_veins_[minitile_x][minitile_y] > 0);
                    bool local_tile_2_1 = (map_veins_[(minitile_x + 1)][minitile_y] < iter  && map_veins_[(minitile_x + 1)][minitile_y]> 0);

                    bool local_tile_0_2 = (map_veins_[(minitile_x - 1)][(minitile_y + 1)]  < iter  && map_veins_[(minitile_x - 1)][(minitile_y + 1)] > 0);
                    bool local_tile_1_2 = (map_veins_[minitile_x][(minitile_y + 1)]        < iter  && map_veins_[minitile_x][(minitile_y + 1)] > 0);
                    bool local_tile_2_2 = (map_veins_[(minitile_x + 1)][(minitile_y + 1)]  < iter  && map_veins_[(minitile_x + 1)][(minitile_y + 1)] > 0);

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_tile_0_0 && (local_tile_2_2 || local_tile_2_1 || local_tile_1_2)) ||
                        (local_tile_1_0 && (local_tile_1_2 || local_tile_0_2 || local_tile_2_2)) ||
                        (local_tile_2_0 && (local_tile_0_2 || local_tile_0_1 || local_tile_1_2)) ||
                        (local_tile_0_1 && (local_tile_2_1 || local_tile_2_0 || local_tile_2_2));

                    bool open_path =
                        (!local_tile_0_0 && !local_tile_2_2) ||
                        (!local_tile_1_0 && !local_tile_1_2) ||
                        (!local_tile_2_0 && !local_tile_0_2) ||
                        (!local_tile_0_1 && !local_tile_2_1) ;

                    bool adjacent_tiles =
                        local_tile_0_0 && local_tile_0_1 && local_tile_0_2 || // left edge
                        local_tile_2_0 && local_tile_2_1 && local_tile_2_2 || // right edge
                        local_tile_0_0 && local_tile_1_0 && local_tile_2_0 || // bottom edge
                        local_tile_0_2 && local_tile_1_2 && local_tile_2_2 || // top edge
                        local_tile_0_1 && local_tile_1_0 && local_tile_0_0 || // lower left slice.
                        local_tile_0_1 && local_tile_1_2 && local_tile_0_2 || // upper left slice.
                        local_tile_1_2 && local_tile_2_1 && local_tile_2_2 || // upper right slice.
                        local_tile_1_0 && local_tile_2_1 && local_tile_2_0; // lower right slice.

                    if ( open_path && opposing_tiles ) {  //mark chokes when found.
                        map_veins_[minitile_x][minitile_y] = 299 - iter;
                    }
                    else if ( (!open_path && opposing_tiles) || adjacent_tiles ) { //if it is closing off in any other way than "formal choke"- it's just a bad place to walk. Mark as unwalkable and continue. Will seal map.
                        map_veins_[minitile_x][minitile_y] = iter;
                    }
                }
            }
        }
    }
}

void Inventory::updateMapVeinsOut() { //in progress.

    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    WalkPosition startloc = WalkPosition( Broodwar->self()->getStartLocation() );

    // first, define matrixes to recieve the smoothed locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            if ( WalkPosition(x,y) == startloc )  {
                temp.push_back( 999999999 );
            }
            else {
                temp.push_back( map_veins_[x][y] );  // copy map veins
            }
        }
        map_veins_out_.push_back( temp );
    }

        int minitile_x, minitile_y, dx, dy, dist_to_x_edge, dist_to_y_edge, normalized_x, normalized_y;
        minitile_x = startloc.x;
        minitile_y = startloc.y;
        dist_to_x_edge  = std::max( map_x - minitile_x, minitile_x );
        dist_to_y_edge = std::max( map_y - minitile_y, minitile_y );

        int t = std::max( map_x + dist_to_x_edge, map_y + dist_to_y_edge );

        dx = dist_to_x_edge < dist_to_y_edge ? -1 : 0;
        dy = dist_to_x_edge <= dist_to_y_edge ? 0 : -1; // start going towards the emptier direction.

        int maxI = t*t; // total number of spiral steps we have to make.

        for ( int i = 0; i < maxI; i++ ) {
            if ( ( 0 < minitile_x) && (minitile_x < map_x ) && ( 0 < minitile_y) && (minitile_y < map_y ) ) { // if you are on the map, continue.

                if ( map_veins_out_[minitile_x][minitile_y] > 175 ) { // if it is walkable, consider it a canidate for a choke.
                    //int min_observed = 100000;
                    //for ( int local_x = -1; local_x <= 1; ++local_x ) {
                    //    for ( int local_y = -1; local_y <= 1; ++local_y ) {
                    //        int testing_x = minitile_x + local_x;
                    //        int testing_y = minitile_y + local_y;
                    //        if ( !(local_x == 0 && local_y == 0) &&
                    //            testing_x < map_x &&
                    //            testing_y < map_y &&
                    //            testing_x > 0 &&
                    //            testing_y > 0 ) { // check for being within reference space.

                    //            int temp = map_chokes_[testing_x][testing_y];
                    //            if ( temp > 0 && temp < min_observed ) {
                    //                map_chokes_[minitile_x][minitile_y] = temp - 1;
                    //                min_observed = temp;
                    //            }
                    //        }
                    //    }
                    //}
                    map_veins_out_[minitile_x][minitile_y] = 99999 - i;
                }
            }

            normalized_x = minitile_x - startloc.x;
            normalized_y = minitile_y - startloc.y;

            if ( normalized_x == normalized_y || ((normalized_x < 0) && ( normalized_x == - normalized_y )) || ((normalized_x > 0) && (normalized_x == 1-normalized_y) ) ) {
                t = dx; // using t as a temp.
                dx = -dy;
                dy = t;
            }

            minitile_x += dx;
            minitile_y += dy;
        }

}

void Inventory::updateLiveMapVeins( const Unit &building, const Unit_Inventory &ui, const Unit_Inventory &ei ) { // in progress.
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles
    int area_modified = 100 * 8;


    //modified areas stopping at bounds. bounds are 1 inside edge of map.
    WalkPosition max_lower_right = WalkPosition( Position( building->getPosition().x + area_modified, building->getPosition().y + area_modified ));
    WalkPosition max_upper_left = WalkPosition( Position( building->getPosition().x - area_modified, building->getPosition().y - area_modified ));

    WalkPosition lower_right_modified = WalkPosition(max_lower_right.x < map_x ? max_lower_right.x : map_x - 1, max_lower_right.y < map_y ? max_lower_right.y : map_y - 1  );
    WalkPosition upper_left_modified =  WalkPosition(max_upper_left.x > 0 ? max_upper_left.x : 1, max_upper_left.y > 0 ? max_upper_left.y : 1  );

// clear tiles that may have been altered.
    for ( auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x ) {
        for ( auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y ) { // Check all possible walkable locations.
                if ( smoothed_barriers_[minitile_x][minitile_y] == 0 ) {
                    Position pos = Position( WalkPosition( minitile_x, minitile_y ) );
                        if ( MeatAIModule::checkBuildingOccupiedArea(ui,pos) || MeatAIModule::checkBuildingOccupiedArea( ei, pos ) ) {
                            map_veins_[minitile_x][minitile_y] = 1;
                        }
                        else /*if ( MeatAIModule::checkUnitOccupiesArea( building, pos, area_modified ) )*/ {
                            map_veins_[minitile_x][minitile_y] = 0; // if it is nearby nuke it to 0 for recasting.
                        }
                }
            }
        }

    for ( auto iter = 2; iter < 175; iter++ ) { // iteration 1 is already done by labling unwalkables. Less loops are needed because most of the map is already plotted.
        for ( auto minitile_x = upper_left_modified.x; minitile_x <= lower_right_modified.x; ++minitile_x ) {
            for ( auto minitile_y = upper_left_modified.y; minitile_y <= lower_right_modified.y; ++minitile_y ) { // Check all possible walkable locations.

            //Psudocode: if any two opposing points are unwalkable, while an alternative path through the center is walkable, it is a choke, the fewer cycles it takes to identify this, the tigher the choke.
            //    If any 3 points adjacent are unwalkable it is probably just a bad place to walk, dead end, etc.Mark it as unwalkable.Do not consider it unwalkable this cycle.
            //    if any corner of it is inaccessable, it is a diagonal wall, mark it as unwalkable.Do not consider it unwalkable this cycle.
            //        Repeat untill finished.

                Position pos = Position( WalkPosition( minitile_x, minitile_y ) );

                if ( map_veins_[minitile_x][minitile_y] == 0 ) { // if it is walkable, consider it a canidate for a choke.

                    // Predefine grid we will search over.
                    bool local_tile_0_0 = (map_veins_[(minitile_x - 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x - 1)][(minitile_y - 1)] > 0);
                    bool local_tile_1_0 = (map_veins_[minitile_x][(minitile_y - 1)] < iter && map_veins_[minitile_x][(minitile_y - 1)] > 0);
                    bool local_tile_2_0 = (map_veins_[(minitile_x + 1)][(minitile_y - 1)] < iter && map_veins_[(minitile_x + 1)][(minitile_y - 1)] > 0);

                    bool local_tile_0_1 = (map_veins_[(minitile_x - 1)][minitile_y] < iter  && map_veins_[(minitile_x - 1)][minitile_y] > 0);
                    bool local_tile_1_1 = (map_veins_[minitile_x][minitile_y] < iter  && map_veins_[minitile_x][minitile_y] > 0);
                    bool local_tile_2_1 = (map_veins_[(minitile_x + 1)][minitile_y] < iter  && map_veins_[(minitile_x + 1)][minitile_y]> 0);

                    bool local_tile_0_2 = (map_veins_[(minitile_x - 1)][(minitile_y + 1)] < iter  && map_veins_[(minitile_x - 1)][(minitile_y + 1)] > 0);
                    bool local_tile_1_2 = (map_veins_[minitile_x][(minitile_y + 1)] < iter  && map_veins_[minitile_x][(minitile_y + 1)] > 0);
                    bool local_tile_2_2 = (map_veins_[(minitile_x + 1)][(minitile_y + 1)] < iter  && map_veins_[(minitile_x + 1)][(minitile_y + 1)] > 0);

                    // if it is surrounded, it is probably a choke, with weight inversely proportional to the number of cycles we have taken this on.
                    bool opposing_tiles =
                        (local_tile_0_0 && (local_tile_2_2 || local_tile_2_1 || local_tile_1_2)) ||
                        (local_tile_1_0 && (local_tile_1_2 || local_tile_0_2 || local_tile_2_2)) ||
                        (local_tile_2_0 && (local_tile_0_2 || local_tile_0_1 || local_tile_1_2)) ||
                        (local_tile_0_1 && (local_tile_2_1 || local_tile_2_0 || local_tile_2_2));

                    bool open_path =
                        (!local_tile_0_0 && !local_tile_2_2) ||
                        (!local_tile_1_0 && !local_tile_1_2) ||
                        (!local_tile_2_0 && !local_tile_0_2) ||
                        (!local_tile_0_1 && !local_tile_2_1);

                    bool adjacent_tiles =
                        local_tile_0_0 && local_tile_0_1 && local_tile_0_2 || // left edge
                        local_tile_2_0 && local_tile_2_1 && local_tile_2_2 || // right edge
                        local_tile_0_0 && local_tile_1_0 && local_tile_2_0 || // bottom edge
                        local_tile_0_2 && local_tile_1_2 && local_tile_2_2 || // top edge
                        local_tile_0_1 && local_tile_1_0 && local_tile_0_0 || // lower left slice.
                        local_tile_0_1 && local_tile_1_2 && local_tile_0_2 || // upper left slice.
                        local_tile_1_2 && local_tile_2_1 && local_tile_2_2 || // upper right slice.
                        local_tile_1_0 && local_tile_2_1 && local_tile_2_0; // lower right slice.

                    if ( open_path && opposing_tiles ) {  //mark chokes when found.
                        map_veins_[minitile_x][minitile_y] = 299 - iter;
                    }
                    else if ( (!open_path && opposing_tiles) || adjacent_tiles ) { //if it is closing off in any other way than "formal choke"- it's just a bad place to walk. Mark as unwalkable and continue. Will seal map.
                        map_veins_[minitile_x][minitile_y] = iter;
                    }

                }
            }
        }
    }
}

void Inventory::updateMapChokes() { 
    int map_x = Broodwar->mapWidth() * 4;
    int map_y = Broodwar->mapHeight() * 4; //tile positions are 32x32, walkable checks 8x8 minitiles. 
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );

    // first, define matrixes to recieve the smoothed locations for every minitile.
    for ( int x = 0; x <= map_x; ++x ) {
        vector<int> temp;
        for ( int y = 0; y <= map_y; ++y ) {
            temp.push_back( 0 );
        }
        map_chokes_.push_back( temp );
    }

    for ( auto minitile_x = 1; minitile_x <= map_x; ++minitile_x ) {
        for ( auto minitile_y = 1; minitile_y <= map_y; ++minitile_y ) { // Check all possible walkable locations.

            int max_observed = map_veins_[minitile_x][minitile_y];
            int counter = 0;

            if ( smoothed_barriers_[minitile_x][minitile_y] == 0 ) {
                for ( int x = -1; x <= 1; ++x ) {
                    for ( int y = -1; y <= 1; ++y ) {
                        int testing_x = minitile_x + x;
                        int testing_y = minitile_y + y;
                        if ( !(x == 0 && y == 0) &&
                            testing_x < map_dim.x &&
                            testing_y < map_dim.y &&
                            testing_x > 0 &&
                            testing_y > 0 ) { // check for being within reference space.

                            if ( map_veins_[testing_x][testing_y] <= max_observed ) {
                                counter++;
                                if ( counter == 8 ) {
                                    map_chokes_[minitile_x][minitile_y] = 300-map_veins_[minitile_x][minitile_y];
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


void Inventory::updateBaseLoc(const Resource_Inventory &ri) {

	int map_x = Broodwar->mapWidth();
	int map_y = Broodwar->mapHeight();
	int location_quality = 0;
	int residual_sq = 0;
	int resources_stored = 0;
	int search_field = 7;

	// first, define matrixes to recieve the base locations. 0 if unbuildable, 1 if buildable.
	for (int x = 0; x <= map_x; ++x) {
		vector<int> temp;
		for (int y = 0; y <= map_y; ++y) {
			temp.push_back((int)Broodwar->isBuildable(x, y)); // explicit converion.
		}
		base_values_.push_back(temp);
	}


	for (auto p = ri.resource_inventory_.begin(); p != ri.resource_inventory_.end(); ++p) { // search for closest resource group. They are our potential expos.

		TilePosition min_pos_t;

		if (p->second.type_.isMineralField()){
			int centralized_resource_x = p->second.pos_.x + 0.5 * UnitTypes::Resource_Mineral_Field.width();
			int centralized_resource_y = p->second.pos_.y + 0.5 * UnitTypes::Resource_Mineral_Field.height();
			min_pos_t = TilePosition(Position(centralized_resource_x, centralized_resource_y));
		}
		else {
			int centralized_resource_x = p->second.pos_.x + 0.5 * UnitTypes::Resource_Vespene_Geyser.width();
			int centralized_resource_y = p->second.pos_.y + 0.5 * UnitTypes::Resource_Vespene_Geyser.height();
			min_pos_t = TilePosition(Position(centralized_resource_x, centralized_resource_y));
		}

		for (auto possible_base_tile_x = min_pos_t.x - 7; possible_base_tile_x != min_pos_t.x + 7; ++possible_base_tile_x) {
			for (auto possible_base_tile_y = min_pos_t.y - 7; possible_base_tile_y != min_pos_t.y + 7; ++possible_base_tile_y) { // Check wide area of possible build locations around each mineral.

				if (possible_base_tile_x >= 0 && possible_base_tile_x <= map_x &&
					possible_base_tile_y >= 0 && possible_base_tile_y <= map_y) { // must be in the map bounds 

					TilePosition prosepective_location = { possible_base_tile_x + 2, possible_base_tile_y + 1 }; // The build location is upper left tile of the building. The origin tile is 0,0. This is the center tile.

					if ((prosepective_location.x >= min_pos_t.x + 2 ||
						prosepective_location.x <= min_pos_t.x - 2 ||
						prosepective_location.y >= min_pos_t.y + 2 ||
						prosepective_location.y <= min_pos_t.y - 2) && // it cannot be within 3 of the origin resource. This discards many distant expos.
						Broodwar->canBuildHere(TilePosition(possible_base_tile_x, possible_base_tile_y), UnitTypes::Zerg_Hatchery) &&
						MeatAIModule::isClearRayTrace(Position(prosepective_location), Position(min_pos_t), *this)) { // if it is 3 away from the resource, and has clear vision to the resource.

						int local_min = 0;

						for (auto j = ri.resource_inventory_.begin(); j != ri.resource_inventory_.end(); ++j) {

							TilePosition tile_resource_position;

							if (j->second.type_.isMineralField()){
								int local_resource_x = j->second.pos_.x + 0.5 * UnitTypes::Resource_Mineral_Field.width();
								int local_resource_y = j->second.pos_.y + 0.5 * UnitTypes::Resource_Mineral_Field.height();
								tile_resource_position = TilePosition(Position(local_resource_x, local_resource_y));
							}
							else {
								int local_resource_x = j->second.pos_.x + 0.5 * UnitTypes::Resource_Vespene_Geyser.width();
								int local_resource_y = j->second.pos_.y + 0.5 * UnitTypes::Resource_Vespene_Geyser.height();
								tile_resource_position = TilePosition(Position(local_resource_x, local_resource_y));
							}

							bool long_condition = tile_resource_position.x >= prosepective_location.x - search_field - 2 &&
								tile_resource_position.x <= prosepective_location.x + search_field &&
								tile_resource_position.y >= prosepective_location.y - search_field - 1 &&
								tile_resource_position.y <= prosepective_location.y + search_field + 1;

							if (long_condition) {
								//residual_sq += pow(Position( TilePosition(possible_base_tile_x, possible_base_tile_y) ).getDistance(Position(tile_resource_position)) / 32, 2); //in minitiles of distance
								resources_stored += j->second.current_stock_value_ - Position(prosepective_location).getDistance(Position(tile_resource_position)) / 32;
								++local_min;
							}

						}

						if (local_min >= 3){
							location_quality = resources_stored;
						}

					}
					else {
						location_quality = 0; // redundant, defaults to 0 - but clear.
					} // if it's invalid for some reason return 0.

					base_values_[possible_base_tile_x][possible_base_tile_y] = location_quality;

					residual_sq = 0; // clear so i don't over-aggregate
					resources_stored = 0;

				} // closure in bounds

			}
		}
	}
}

void Inventory::updateReserveSystem() {
    if ( Broodwar->getFrameCount() == 0 ) {
        min_reserve_= 0;
        gas_reserve_ = 0;
        building_timer_ = 0;
    }
    else {
        building_timer_ > 0 ? --building_timer_ : 0;
    }
}


void Inventory::updateNextExpo(const Unit_Inventory &e_inv, const Unit_Inventory &u_inv) {

	TilePosition center_self = TilePosition(u_inv.getMeanBuildingLocation());
	int location_qual_threshold = -999999;
	acceptable_expo_ = false;
	Region home = Broodwar->getRegionAt(Position(center_self));
	Regionset neighbors;
	bool local_maximum = true;

	neighbors.insert(home);

	Unit_Inventory bases = MeatAIModule::getUnitInventoryInRadius(u_inv, UnitTypes::Zerg_Hatchery, Position(center_self), 9999999);
	for (auto b = bases.unit_inventory_.begin(); b != bases.unit_inventory_.end() && !bases.unit_inventory_.empty(); b++){
		home = Broodwar->getRegionAt(b->second.pos_);
		Regionset new_neighbors = home->getNeighbors();
		for (auto r = new_neighbors.begin(); r != new_neighbors.end() && !new_neighbors.empty(); r++) {
			if ((*r)->isAccessible()){ neighbors.insert(*r); }
		}
	}
	bases = MeatAIModule::getUnitInventoryInRadius(u_inv, UnitTypes::Zerg_Lair, Position(center_self), 9999999);
	for (auto b = bases.unit_inventory_.begin(); b != bases.unit_inventory_.end() && !bases.unit_inventory_.empty(); b++){
		home = Broodwar->getRegionAt(b->second.pos_);
		Regionset new_neighbors = home->getNeighbors();
		for (auto r = new_neighbors.begin(); r != new_neighbors.end() && !new_neighbors.empty(); r++) {
			if ((*r)->isAccessible()){ neighbors.insert(*r); }
		}
	}
	bases = MeatAIModule::getUnitInventoryInRadius(u_inv, UnitTypes::Zerg_Hive, Position(center_self), 9999999);
	for (auto b = bases.unit_inventory_.begin(); b != bases.unit_inventory_.end() && !bases.unit_inventory_.empty(); b++){
		home = Broodwar->getRegionAt(b->second.pos_);
		Regionset new_neighbors = home->getNeighbors();
		for (auto r = new_neighbors.begin(); r != new_neighbors.end() && !new_neighbors.empty(); r++) {
			if ((*r)->isAccessible()){ neighbors.insert(*r); }
		}
	}

	for (vector<int>::size_type x = 0; x != base_values_.size(); ++x) {
		for (vector<int>::size_type y = 0; y != base_values_[x].size(); ++y) {
			if (base_values_[x][y] > 1) { // only consider the decent locations please.

				TilePosition canidate_spot = TilePosition(x + 2, y + 1); // from the true center of the object.
				int walk = Position(canidate_spot).getDistance(Position(center_self)) / 32;
				int net_quality = base_values_[x][y] - pow(Position(canidate_spot).getDistance(Position(center_self)) / 32, 2); //value of location and distance from our center.  Plus some terms so it's positive, we like to look at positive numbers.

				bool enemy_in_inventory_near_expo = false; // Don't build on enemies!
				bool found_rdepot = false;
				bool is_neighboring_region = false;

				Unit_Inventory e_loc = MeatAIModule::getUnitInventoryInRadius(e_inv, Position(canidate_spot), 500);
				e_loc.updateUnitInventorySummary();
				if (e_loc.stock_shoots_down_ + e_loc.stock_shoots_up_ > 0) { //if they have any combat units nearby.
					enemy_in_inventory_near_expo = true;
				}

				Unit_Inventory rdepot = MeatAIModule::getUnitInventoryInRadius(u_inv, Position(canidate_spot), 500);
				for (const auto &r : rdepot.unit_inventory_) {
					if (r.second.type_.isResourceDepot()) {
						found_rdepot = true;
					}
				}

				//Region canidate_region = Broodwar->getRegionAt(Position(canidate_spot));
				//if (neighbors.contains(canidate_region)){
				//	is_neighboring_region = true;
				//}
				//else {
				//	int canidate_group = canidate_region->getRegionGroupID();
				//	for (auto r = neighbors.begin(); r != neighbors.end() && !neighbors.empty(); r++) {
				//		if ( (*r) && (*r)->getRegionGroupID() == canidate_group) {
				//			//is_neighboring_region = true;
				//		}
				//	}
				//}
				for (int i = -7; i <= 7; i++){
					for (int j = -7; j <= 7; j++){
						bool safety_check = x + i < base_values_.size() && x - i > 0 && y + j < base_values_[x + i].size() && y - j > 0;
						if (safety_check && base_values_[x][y] < base_values_[x + i][y + j]){
							local_maximum = false;
							break;
						}
					}
				}


				bool condition = net_quality >= location_qual_threshold && !enemy_in_inventory_near_expo && !found_rdepot && local_maximum;

				if (condition) {
					next_expo_ = { static_cast<int>(x), static_cast<int>(y) };
					acceptable_expo_ = true;
					location_qual_threshold = net_quality;
				}
				local_maximum = true;
			}

		} // closure y
	} // closure x
}

void Inventory::getStartPositions(){
	for (auto loc : Broodwar->getStartLocations()){
		start_positions_.push_back(Position(loc));
	}
	//std::vector<Position> v{ std::begin(Broodwar->getStartLocations()), std::end(Broodwar->getStartLocations()) };
	//start_positions_ = v;
}

void Inventory::updateStartPositions(){
	for (auto visible_base = start_positions_.begin(); visible_base != start_positions_.end() && !start_positions_.empty();){
		if (Broodwar->isExplored(TilePosition(*visible_base))){
			start_positions_.erase(visible_base);
		}
		else {
			++visible_base;
		}
	}
	if (start_positions_.empty()){
		list_cleared_ = true;
	}
}
//Zerg_Zergling, 37
//Zerg_Hydralisk, 38
//Zerg_Ultralisk, 39
//Zerg_Broodling, 40
//Zerg_Drone, 41
//Zerg_Overlord, 42
//Zerg_Mutalisk, 43
//Zerg_Guardian, 44
//Zerg_Queen, 45
//Zerg_Defiler, 46
//Zerg_Scourge, 47
