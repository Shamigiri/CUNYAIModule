#pragma once

# include "Source\MeatAIModule.h"
# include "Source\Fight_MovementManager.h"


using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a boids manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.
void Boids::Boids_Movement( const Unit &unit, const double &n, const Unit_Inventory &ui, const Unit_Inventory &ei, const Inventory &inventory ) {

    Position pos = unit->getPosition();
    Unit_Inventory flock = MeatAIModule::getUnitInventoryInRadius( ui, pos, 1024 );

    setAlignment( unit, flock );
    setCentralize( pos, inventory );
    setStutter( unit, n );
    setCohesion( unit, pos, flock );
    setAttraction( unit, pos, ei );

    // The following do NOT apply to flying units: Seperation, unwalkability.
    if ( !unit->getType().isFlyer() ) {
        Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius( ui, pos, 32 );
        setSeperation( unit, pos, neighbors );
        setUnwalkability( unit, pos, inventory );
    } // closure: flyers

      //Make sure the end destination is one suitable for you.
    Position brownian_pos = { (int)(pos.x + x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), (int)(pos.y + y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };

    bool walkable_plus = brownian_pos.isValid() &&
        (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
        (inventory.smoothed_barriers_[brownian_pos.x / 8][brownian_pos.y / 8] == 0)); //or is a relatively walkable position? 

    if ( !walkable_plus ) { // if we can't move there for some reason, Push from unwalkability, tilted 1/4 pi, 45 degrees, we'll have to go around the obstruction.
        setObjectAvoid( unit, pos, inventory );
        brownian_pos = { (int)(pos.x + x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), (int)(pos.y + y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };// redefine this to be a walkable one.
    }

    if ( unit->canAttack( brownian_pos ) ) {
        unit->attack( brownian_pos );
    }
    else {
        unit->move( brownian_pos );
    }

    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attune_dx_)       , (int)(pos.y + attune_dy_) }, Colors::Green );//Alignment
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, Colors::Blue ); // Centraliziation.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + cohesion_dx_)     , (int)(pos.y + cohesion_dy_) }, Colors::Purple ); // Cohesion
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attract_dx_)      , (int)(pos.y + attract_dy_) }, Colors::Red ); //Attraction towards attackable enemies.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x - seperation_dx_)   , (int)(pos.y - seperation_dy_) }, Colors::Orange ); // Seperation, does not apply to fliers.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x - walkability_dx_)  , (int)(pos.y - walkability_dy_) }, Colors::Grey ); // Push from unwalkability, different regions. May tilt to become parallel with obstructions to get around them.

};

// This is basic combat logic for nonspellcasting units.
void Boids::Tactical_Logic( const Unit &unit, const Unit_Inventory &ei, const Color &color = Colors::White )
{
    UnitType u_type = unit->getType();
    Unit target;
    int priority = 0;

    int range_radius = u_type.airWeapon().maxRange() > u_type.groundWeapon().maxRange() ? u_type.airWeapon().maxRange() : u_type.groundWeapon().maxRange();
    int dist = 32 + range_radius;
    int max_dist = ei.max_range_/(int)ei.unit_inventory_.size();
    int max_dist_no_priority = 9999999;
    bool target_sentinel = false;
    bool target_sentinel_wimpy_atk = false;


    for ( auto e = ei.unit_inventory_.begin(); e != ei.unit_inventory_.end() && !ei.unit_inventory_.empty(); ++e ) {
        if ( e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() ) {
            UnitType e_type = e->second.type_;
            int e_priority = 0;

            if ( MeatAIModule::Can_Fight( unit, e->second.bwapi_unit_ ) ) { // if we can fight this enemy and there is a clear path to them:
                int dist_to_enemy = unit->getDistance( e->second.bwapi_unit_ );
                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    e->second.current_hp_ < 0.25 * e_type.maxHitPoints() && MeatAIModule::Can_Fight( e->second.bwapi_unit_, unit ) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units

                if ( critical_target ) {
                    e_priority = 3;
                }
                else if ( MeatAIModule::Can_Fight( e->second.bwapi_unit_, unit ) || e_type.spaceProvided() > 0 || e->second.bwapi_unit_->isAttacking() || (e_type.isSpellcaster() && !e_type.isBuilding()) || e->second.bwapi_unit_->isRepairing() || e_type == UnitTypes::Protoss_Carrier ) { // if they can fight us, carry troops, or cast spells.
                    e_priority = 2;
                }
                else if ( e->second.type_.mineralPrice() > 0 ) {
                    e_priority = 1; // or if they cant fight back we'll get those last.
                }
                else {
                    e_priority = 0; // should leave stuff like larvae and eggs in here. Low, low priority.
                }

                if ( (e_priority == priority && dist_to_enemy <= dist) || e_priority > priority && dist_to_enemy < max_dist ) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
                    target_sentinel = true;
                    priority = e_priority;
                    dist = dist_to_enemy; // now that we have one within range, let's tighten our existing range.
                    target = e->second.bwapi_unit_;
                }
                else if ( !target_sentinel && dist_to_enemy >= max_dist && dist_to_enemy < max_dist_no_priority ) {
                    target_sentinel_wimpy_atk = true;
                    dist = dist_to_enemy; // if nothing is within range, let's take any old target. We do not look for priority among these, merely closeness. helps melee units lock onto target instead of diving continually into enemy lines.
                    max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                    target = e->second.bwapi_unit_;
                }
            }
        }
    }

    if ( (target_sentinel || target_sentinel_wimpy_atk) && target && target->exists() ) {
        unit->attack( target );
        MeatAIModule::Diagnostic_Line( unit->getPosition(), target->getPosition(), color );
    }
}

// Basic retreat logic, range = enemy range
void Boids::Retreat_Logic( const Unit &unit, const Unit &enemy, const Unit_Inventory &ui, const Inventory &inventory, const Color &color = Colors::White ) {

    int dist = unit->getDistance( enemy );
    int air_range = enemy->getType().airWeapon().maxRange();
    int ground_range = enemy->getType().groundWeapon().maxRange();
    int range = air_range > ground_range ? air_range : ground_range;
    if ( dist < range + 100 ) { //  Run if you're a noncombat unit or army starved. +100 for safety. Retreat function now accounts for walkability.

        Position e_pos = enemy->getPosition();

        Position pos = unit->getPosition();
        Unit_Inventory flock = MeatAIModule::getUnitInventoryInRadius( ui, pos, 1024 );

        Broodwar->drawCircleMap( e_pos, range, Colors::Red );

        //initial retreat spot from enemy.
        int dist_x = e_pos.x - pos.x;
        int dist_y = e_pos.y - pos.y;
        double theta = atan2( dist_y, dist_x ); // att_y/att_x = tan (theta).
        double retreat_dx = -cos( theta ) * (2 * range - dist);
        double retreat_dy = -sin( theta ) * (2 * range - dist); // get -range- outside of their range.  Should be safe.

        setAlignment( unit, ui );
        setCentralize( pos, inventory );
        setCohesion( unit, pos, ui );

        // The following do NOT apply to flying units: Seperation, unwalkability.
        if ( !unit->getType().isFlyer() ) {
            Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius( ui, pos, 32 );
            setSeperation( unit, pos, neighbors );
            setUnwalkability( unit, pos, inventory );
        } // closure: flyers


          //Make sure the end destination is one suitable for you.
        Position retreat_spot = { (int)(pos.x + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), (int)(pos.y + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };

        bool walkable_plus = retreat_spot.isValid() &&
            (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
            (inventory.smoothed_barriers_[retreat_spot.x / 8][retreat_spot.y / 8] == 0)); //or is a relatively walkable position? 

        if ( !walkable_plus ) { // if we can't move there for some reason, Push from unwalkability, tilted 1/4 pi, 45 degrees, we'll have to go around the obstruction.
            setObjectAvoid( unit, pos, inventory );
            retreat_spot = { (int)(pos.x + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), (int)(pos.y + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };
            // redefine this to be a walkable one.
        }

        if ( retreat_spot && retreat_spot.isValid() ) {
            unit->move( retreat_spot ); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
            MeatAIModule::Diagnostic_Line( pos, retreat_spot, color );
        }
    }
}


//Brownian Stuttering, causes unit to move about randomly.
void Boids::setStutter( const Unit &unit, int n ) {
    x_stutter_ = n * (rand() % 101 - 50) / 50 * unit->getType().sightRange();
    y_stutter_ = n * (rand() % 101 - 50) / 50 * unit->getType().sightRange();// The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.
}

//Alignment. Convinces all units in unit inventory to move at similar velocities.
void Boids::setAlignment( Unit unit, Unit_Inventory ui ) {
    int temp_tot_x = 0;
    int temp_tot_y = 0;

    int flock_count = 0;
    if ( !ui.unit_inventory_.empty() ) {
        for ( auto i = ui.unit_inventory_.begin(); i != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); ++i ) {
            if ( i->second.type_ != UnitTypes::Zerg_Drone && i->second.type_ != UnitTypes::Zerg_Overlord && i->second.type_ != UnitTypes::Buildings ) {
                temp_tot_x += i->second.bwapi_unit_->getVelocityX() ; //get the horiz element.
                temp_tot_y += i->second.bwapi_unit_->getVelocityY() ; // get the vertical element. Averaging angles was trickier than I thought. 
                flock_count++;
            }
        }
        double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.
        attune_dx_ = cos( theta ) * 0.95 * temp_tot_x/flock_count * 48;
        attune_dy_ = sin( theta ) * 0.95 * temp_tot_y/flock_count * 48; // think the velocity is per frame, I'd prefer it per second.
    }
}

//Centralization, all units prefer locations with mobility to edges.
void Boids::setCentralize( Position pos, Inventory inventory ) {
    int temp_centralization_dx_ = 0;
    int temp_centralization_dy_ = 0;
    for ( int x = -5; x <= 5; ++x ) {
        for ( int y = -5; y <= 5; ++y ) {
            double centralize_x = pos.x / 8 + x;
            double centralize_y = pos.y / 8 + y;
            if ( !(x == 0 && y == 0) &&
                centralize_x < Broodwar->mapWidth() && centralize_y < Broodwar->mapHeight() &&
                centralize_x > 0 && centralize_y > 0 &&
                inventory.smoothed_barriers_[centralize_x][centralize_y] == 0 ) {
                double theta = atan2( centralize_y - pos.y / 8, centralize_x - pos.x / 8 );
                temp_centralization_dx_ += cos( theta );
                temp_centralization_dy_ += sin( theta );
            }
        }
    }
    if ( temp_centralization_dx_ != 0 && temp_centralization_dy_ != 0 ) {
        double theta = atan2( temp_centralization_dy_, temp_centralization_dx_ );
        int temp_dist = abs( temp_centralization_dx_ * 8 ) + abs( temp_centralization_dy_ * 8 ); // we should move closer based on the number of choke tiles.
        centralization_dx_ = cos( theta ) * temp_dist;
        centralization_dy_ = sin( theta ) * temp_dist;
    }
}

//Cohesion, all units tend to prefer to be together.
void Boids::setCohesion( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {

    const Position loc_center = ui.getMeanLocation();
    double cohesion_x = loc_center.x - pos.x;
    double cohesion_y = loc_center.y - pos.y;
    double theta = atan2( cohesion_y, cohesion_x );
    cohesion_dx_ = cos( theta ) * 0.10 * unit->getDistance( loc_center );
    cohesion_dy_ = sin( theta ) * 0.10 * unit->getDistance( loc_center );

}

//Attraction, pull towards enemy units that we can attack.
void Boids::setAttraction( const Unit &unit, const Position &pos, const Unit_Inventory &ei ) {
    if ( (unit->getType().airWeapon() != WeaponTypes::None || unit->getType().groundWeapon() != WeaponTypes::None) ) {
        // (!army_starved || inventory.est_enemy_stock_ <= 0.75 * exp( inventory.ln_army_stock_ )) && unit->getHitPoints() > 0.25 * unit->getType().maxHitPoints() 

        int dist = 999999;
        bool visible_unit_found = false;
        if ( !ei.unit_inventory_.empty() ) { // if there isn't a visible targetable enemy, but we have an inventory of them...
            Stored_Unit e = ei.unit_inventory_.begin()->second; // just initialize it with any old enemy.

            for ( auto e_iter = ei.unit_inventory_.begin(); e_iter != ei.unit_inventory_.end(); e_iter++ ) {
                int dist_current = unit->getDistance( e_iter->second.pos_ );
                bool visible = Broodwar->isVisible( e.pos_.x / 32, e.pos_.y / 32 );
                if ( visible && dist_current < dist ) {
                    dist = dist_current;
                    Stored_Unit e = e_iter->second;
                    if ( !visible_unit_found ) {
                        visible_unit_found = true;
                        ei.unit_inventory_.begin(); // seems inefficient but is faster than two loops with break to manage the double criteria.  If we have a visible one, start again at the beginning.
                    }
                }
                else if ( !visible_unit_found && dist_current < dist ) {
                    dist = dist_current;
                    Stored_Unit e = e_iter->second;
                }
            } // get closest enemy unit in inventory.

            dist = 999999;
            visible_unit_found = false; // don't want to be stuck on a closeby enemy you can't attack.

            if ( unit->getType().airWeapon() == WeaponTypes::None && unit->getType().groundWeapon() != WeaponTypes::None ) {
                for ( auto e_iter = ei.unit_inventory_.begin(); e_iter != ei.unit_inventory_.end(); e_iter++ ) {
                    int dist_current = unit->getDistance( e_iter->second.pos_ );
                    bool visible = Broodwar->isVisible( e.pos_.x / 32, e.pos_.y / 32 );
                    if ( visible && dist_current < dist ) {
                        dist = dist_current;
                        Stored_Unit e = e_iter->second;
                        if ( !visible_unit_found ) {
                            visible_unit_found = true;
                            ei.unit_inventory_.begin(); // seems inefficient but is faster than two loops with break to manage the double criteria.  If we have a visible one, start again at the beginning.
                        }
                    }
                    else if ( !visible_unit_found && dist_current < dist ) {
                        dist = dist_current;
                        Stored_Unit e = e_iter->second;
                    }
                } // get closest non flying enemy unit in inventory.
            }

            dist = 999999;
            visible_unit_found = false;

            if ( unit->getType().airWeapon() != WeaponTypes::None && unit->getType().groundWeapon() == WeaponTypes::None ) {
                for ( auto e_iter = ei.unit_inventory_.begin(); e_iter != ei.unit_inventory_.end(); e_iter++ ) {
                    int dist_current = unit->getDistance( e_iter->second.pos_ );
                    bool visible = Broodwar->isVisible( e.pos_.x / 32, e.pos_.y / 32 );
                    if ( visible && dist_current < dist ) {
                        dist = dist_current;
                        Stored_Unit e = e_iter->second;
                        if ( !visible_unit_found ) {
                            visible_unit_found = true;
                            ei.unit_inventory_.begin(); // seems inefficient but is faster than two loops with break to manage the double criteria.  If we have a visible one, start again at the beginning.
                        }
                    }
                    else if ( !visible_unit_found && dist_current < dist ) {
                        dist = dist_current;
                        Stored_Unit e = e_iter->second;
                    }
                } // get closest flying enemy unit in inventory.
            }

            if ( e.pos_ ) {
                int dist_x = e.pos_.x - pos.x;
                int dist_y = e.pos_.y - pos.y;
                double theta = atan2( dist_y, dist_x );
                attract_dx_ = cos( theta ) * unit->getDistance( e.pos_ ) * 0.30; // run X% towards them.  Must be enough to override the wander.
                attract_dy_ = sin( theta ) * unit->getDistance( e.pos_ ) * 0.30;
            }
        }
    }
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Boids::setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {
    Position seperation = ui.getMeanLocation();
    if ( seperation != pos ) { // don't seperate from yourself, that would be a disaster.
        double seperation_x = seperation.x - pos.x;
        double seperation_y = seperation.y - pos.y;
        double theta = atan2( seperation_y, seperation_x );
        seperation_dx_ = cos( theta ) * 96; // run 3 tiles away from everyone minimum. Should help avoid being stuck in those wonky spots.
        seperation_dy_ = sin( theta ) * 96;
    }
}

void Boids::setUnwalkability( const Unit &unit, const Position &pos, const Inventory &inventory ) {
    //Push from unwalkability, push from different levels.
    int temp_walkability_dx_ = 0;
    int temp_walkability_dy_ = 0;
    for ( int x = -5; x <= 5; ++x ) {
        for ( int y = -5; y <= 5; ++y ) {
            double walkability_x = pos.x + 32 * x; // in pixels
            double walkability_y = pos.y + 32 * y; // in pixels
            if ( !(x == 0 && y == 0) ) {
                if ( walkability_x > Broodwar->mapWidth() * 32 || walkability_y > Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                    walkability_x < 0 || walkability_y < 0 ||  // out of bounds below map,  0.
                    !Broodwar->isWalkable( (int)walkability_x / 8, (int)walkability_y / 8 ) || //Mapheight/width get map dimensions in TILES 1tile=32 pixels. Iswalkable gets it in minitiles, 1 minitile=8 pixels.
                    Broodwar->getGroundHeight( walkability_x / 32, walkability_y / 32 ) != Broodwar->getGroundHeight( unit->getTilePosition() ) ||  //If a position is on a different level, it's essentially unwalkable.
                    inventory.smoothed_barriers_[walkability_x / 8][walkability_y / 8] > 0 ) { // or if the position offers less mobility in general.
                    double theta = atan2( y, x );
                    temp_walkability_dx_ += cos( theta );
                    temp_walkability_dy_ += sin( theta );
                }
            }
        }
    }
    if ( temp_walkability_dx_ != 0 && temp_walkability_dy_ != 0 ) {
        double theta = atan2( temp_walkability_dy_, temp_walkability_dx_ );
        int temp_dist = abs( temp_walkability_dx_ * 8 ) + abs( temp_walkability_dy_ * 8 ); // we should move away based on the number of tiles rejecting us.
        walkability_dx_ = cos( theta ) * temp_dist;
        walkability_dy_ = sin( theta ) * temp_dist;
    }
}
void Boids::setObjectAvoid( const Unit &unit, const Position &pos, const Inventory &inventory ) {

    int temp_walkability_dx_ = 0;
    int temp_walkability_dy_ = 0;
    if ( !unit->isFlying() ) {
        for ( int x = -5; x <= 5; ++x ) {
            for ( int y = -5; y <= 5; ++y ) {
                double walkability_x = pos.x + 32 * x;
                double walkability_y = pos.y + 32 * y;
                if ( !(x == 0 && y == 0) ) {
                    if ( walkability_x > Broodwar->mapWidth() * 32 || walkability_y > Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                        walkability_x < 0 || walkability_y < 0 ||  // out of bounds below map,  0.
                        !Broodwar->isWalkable( (int)walkability_x / 8, (int)walkability_y / 8 ) || //Mapheight/width get map dimensions in TILES 1tile=32 pixels. Iswalkable gets it in minitiles, 1 minitile=8 pixels.
                        Broodwar->getGroundHeight( walkability_x / 32, walkability_y / 32 ) != Broodwar->getGroundHeight( unit->getTilePosition() ) ||  //If a position is on a different level, it's essentially unwalkable.
                        inventory.smoothed_barriers_[walkability_x / 8][walkability_y / 8] > 0 ) { // or if the position is relatively unwalkable.
                        // !Broodwar->getUnitsOnTile( walkability_x / 32, walkability_y / 32, !IsFlying ).empty() ) { // or if the position is occupied.
                        double theta = atan2( y, x );
                        temp_walkability_dx_ += cos( theta );
                        temp_walkability_dy_ += sin( theta );
                    }
                }
            }
        }
    }
    if ( temp_walkability_dx_ != 0 && temp_walkability_dy_ != 0 ) {
        double theta = atan2( temp_walkability_dy_, temp_walkability_dx_ );
        int temp_dist = abs( temp_walkability_dx_ * 8 ) + abs( temp_walkability_dy_ * 8 ); // we should move away based on the number of tiles rejecting us.
        int rng = ((rand() % 150 + 1) / 50 - 1) * 0.25 * 3.1415; // random number -1,0,1 times 0.25 * pi
        walkability_dx_ = cos( theta + rng ) * temp_dist;
        walkability_dy_ = sin( theta + rng ) * temp_dist;
    }
}

//// Drags overlords to the nearest mineral.
//void MeatAIModule::Vision_Locking( const Unit &unit ) {
//    if ( unit->getType() == UnitTypes::Zerg_Overlord ) {
//
//        Position min;
//        int dist = 999999;
//
//        for ( vector<int>::size_type p = 0; p != inventory.resource_positions_.size(); ++p ) { // search for closest resource group. They are our potential expos.
//            int min_dist = inventory.resource_positions_[p].getDistance( unit->getPosition() );
//            if ( min_dist < dist ) {
//                dist = min_dist;
//                Position min = inventory.resource_positions_[p];
//            }
//        }
//
//        Unitset bases = Broodwar->getUnitsInRadius( min, UnitTypes::Zerg_Overlord.sightRange(), IsBuilding && IsOwned );
//        Unitset enemies_loc = Broodwar->getUnitsInRadius( min, UnitTypes::Zerg_Overlord.sightRange(), IsEnemy );
//
//        //Tally up crucial details about enemy. 
//
//        int e_count = enemies_loc.size();
//        int helpless_e = 0;
//        int helpful_e = 0;
//        int helpless_u = 0;
//        int helpful_u = 0;
//        int f_drone = 0;
//
//        for ( auto e = enemies_loc.begin(); e != enemies_loc.end(); e++ ) { // trims the set for each attackable enemy. We move towards the center of these units
//            if ( Can_Fight( unit, *e ) ) {
//                helpful_u++;
//            }
//            if ( Futile_Fight( unit, *e ) ) {
//                helpless_u++;
//            }
//            if ( Can_Fight( *e, unit ) ) {
//                helpful_e++;
//            }
//            if ( Futile_Fight( *e, unit ) ) {
//                helpless_e++;
//            }
//        }
//
//        if ( bases.empty() && helpful_e == 0 ) { // The closest unit aught to approach. Others need not bother.
//            Unit best_unit = Broodwar->getClosestUnit( min );
//            if ( best_unit && best_unit->exists() && unit == best_unit ) {
//                unit->move( min );
//            }
//        }
//    }
//}