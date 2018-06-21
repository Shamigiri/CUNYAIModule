#pragma once
# include "Source\CUNYAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

// Gets units last error and prints it directly onscreen.  From tutorial.
void CUNYAIModule::PrintError_Unit(const Unit &unit) {
    Position pos = unit->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent( [pos, lastErr]( Game* ) { Broodwar->drawTextMap( pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames() );  // frames to run.
}

// Identifies those moments where a worker is gathering and its unusual subcases.
bool CUNYAIModule::isActiveWorker(const Unit &unit){
	bool passive = //BWAPI::Orders::MoveToMinerals &&
		unit->getOrder() == BWAPI::Orders::MoveToGas ||
		unit->getOrder() == BWAPI::Orders::WaitForMinerals ||
		//unit->getOrder() == BWAPI::Orders::WaitForGas && // should never be overstacked on gas.
		unit->getOrder() == BWAPI::Orders::MiningMinerals ||
		unit->getOrder() == BWAPI::Orders::HarvestGas ||
		unit->getOrder() == BWAPI::Orders::ReturnMinerals ||
		unit->getOrder() == BWAPI::Orders::ReturnGas ||
		unit->getOrder() == BWAPI::Orders::ResetCollision;//command is issued promptly when workers finish mining, but must resolve. http://satirist.org/ai/starcraft/blog/archives/220-how-to-beat-Stone,-according-to-AIL.html
	return passive;
}

bool CUNYAIModule::isInLine(const Unit &unit){
	bool passive = 
		unit->getOrder() == BWAPI::Orders::WaitForMinerals ||
		unit->getOrder() == BWAPI::Orders::WaitForGas ||
		unit->getOrder() == BWAPI::Orders::ResetCollision;
	return passive;
}

bool CUNYAIModule::isEmptyWorker(const Unit &unit) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    return !laden_worker;
}
// An improvement on existing idle scripts. Returns true if stuck or finished with most recent task.
bool CUNYAIModule::isIdleEmpty(const Unit &unit) {

    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();

    UnitCommandType u_type = unit->getLastCommand().getType();

	bool task_complete = (u_type == UnitCommandTypes::Move && !unit->isMoving()) ||
                         (u_type == UnitCommandTypes::Morph && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 5 * 24 && !(unit->isMorphing() || unit->isMoving() || unit->isAccelerating())) ||
                         (u_type == UnitCommandTypes::Attack_Move && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Attack_Unit && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Return_Cargo && !laden_worker && !isInLine(unit) ) ||
                         (u_type == UnitCommandTypes::Gather && !unit->isMoving() && !unit->isGatheringGas() && !unit->isGatheringMinerals() && !isInLine(unit)) ||
                         (u_type == UnitCommandTypes::Build && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 5 * 24 && !( unit->isMoving() || unit->isAccelerating() ) ) || // assumes a command has failed if it hasn't executed in the last 10 seconds.
                         (u_type == UnitCommandTypes::Upgrade && !unit->isUpgrading() && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 15 * 24) || // unit is done upgrading.
                         (u_type == UnitCommandTypes::Burrow && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 3 * 24) ||
                         (u_type == UnitCommandTypes::Unburrow && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 3 * 24) ||
                          u_type == UnitCommandTypes::None ||
                          u_type == UnitCommandTypes::Stop ||
						  u_type == UnitCommandTypes::Unknown;


    return ( task_complete || unit->isStuck() ) && !isActiveWorker(unit) && !IsUnderAttack(unit) && spamGuard(unit);
}

// Did the unit fight in the last 5 seconds?
bool CUNYAIModule::isRecentCombatant(const Unit &unit) {
	bool fighting_now = (unit->getLastCommand().getType() == UnitCommandTypes::Attack_Move) || (unit->getLastCommand().getType() == UnitCommandTypes::Attack_Unit);
	bool recent_order = unit->getLastCommandFrame() + 24 > Broodwar->getFrameCount();
	return fighting_now && recent_order;
}

// Checks if a unit is a combat unit.
bool CUNYAIModule::IsFightingUnit(const Unit &unit)
{
    if ( !unit )
    {
        return false;
    }
    UnitType u_type = unit->getType();

    // no workers or buildings allowed. Or overlords, or larva..
    if ( unit && u_type.isWorker() ||
        u_type.isBuilding() ||
        u_type == BWAPI::UnitTypes::Zerg_Larva ||
        u_type == BWAPI::UnitTypes::Zerg_Overlord )
    {
        return false;
    }

    // This is a last minute check for psi-ops. I removed a bunch of these. Observers and medics are not combat units per se.
    if (u_type.canAttack() ||
        u_type == BWAPI::UnitTypes::Protoss_High_Templar ||
        u_type == BWAPI::UnitTypes::Terran_Bunker ||
        unit->isFlying() && u_type.spaceProvided() > 0 )
    {
        return true;
    }

    return false;
}

// Checks if a stored unit is a combat unit.
bool CUNYAIModule::IsFightingUnit(const Stored_Unit &unit)
{
    if (!unit.valid_pos_)
    {
        return false;
    }

    // no workers, overlords, or larva...
    if (unit.type_.isWorker() ||
        //unit.type_.isBuilding() ||
        unit.type_ == BWAPI::UnitTypes::Zerg_Larva ||
        unit.type_ == BWAPI::UnitTypes::Zerg_Overlord)
    {
        return false;
    }

    // This is a last minute check for psi-ops or transports.
    if (unit.type_.canAttack() ||
        unit.type_.maxEnergy() > 0 ||
        unit.type_.isDetector() ||
        unit.type_ == BWAPI::UnitTypes::Terran_Bunker ||
        unit.type_.isFlyer() && unit.type_.spaceProvided() > 0)
    {
        return true;
    }

    return false;
}

// This function limits the drawing that needs to be done by the bot.
void CUNYAIModule::Diagnostic_Line( const Position &s_pos, const Position &f_pos , const Position &screen_pos, Color col = Colors::White ) {
    if ( _ANALYSIS_MODE ) {
        if ( isOnScreen( s_pos , screen_pos) || isOnScreen( f_pos , screen_pos) ) {
            Broodwar->drawLineMap( s_pos, f_pos, col );
        }
    }
}

// This function limits the drawing that needs to be done by the bot.
void CUNYAIModule::Diagnostic_Dot(const Position &s_pos, const Position &screen_pos, Color col = Colors::White) {
    if (_ANALYSIS_MODE) {
        if (isOnScreen(s_pos, screen_pos)) {
            Broodwar->drawCircleMap(s_pos, 25, col, true);
        }
    }
}

void CUNYAIModule::DiagnosticHitPoints(const Stored_Unit unit, const Position &screen_pos) {
    if (_ANALYSIS_MODE && unit.valid_pos_) {
        Position upper_left = unit.pos_;
        if (isOnScreen(upper_left, screen_pos) && unit.current_hp_ != (double)unit.type_.maxHitPoints() + unit.type_.maxShields() ) {
            // Draw the red background.
            upper_left.y = upper_left.y + unit.type_.dimensionUp();
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 10;

            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Red, true);

            //Overlay the appropriate green above it.
            lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width() * unit.current_hp_ / (double) (unit.type_.maxHitPoints() + unit.type_.maxShields()) ;
            lower_right.y = upper_left.y + 10;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Green, true);

            //Overlay the 10hp rectangles over it.
        }
    }
}

void CUNYAIModule::DiagnosticMineralsRemaining(const Stored_Resource resource, const Position &screen_pos) {
    if (_ANALYSIS_MODE) {
        Position upper_left = resource.pos_;
        if (isOnScreen(upper_left, screen_pos) && resource.current_stock_value_ != (double)resource.max_stock_value_ ) {
            // Draw the orange background.
            upper_left.y = upper_left.y + resource.type_.dimensionUp();
            upper_left.x = upper_left.x - resource.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + resource.type_.width();
            lower_right.y = upper_left.y + 10;

            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Orange, true);

            //Overlay the appropriate blue above it.
            lower_right = upper_left;
            lower_right.x = upper_left.x + resource.type_.width() * resource.current_stock_value_ / (double)resource.max_stock_value_;
            lower_right.y = upper_left.y + 10;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Cyan, true);

            //Overlay the 10hp rectangles over it.
        }
    }
}

void CUNYAIModule::DiagnosticSpamGuard(const Stored_Unit unit, const Position & screen_pos)
{
    if (_ANALYSIS_MODE) {
        Position upper_left = unit.pos_;
        if (isOnScreen(upper_left, screen_pos) && unit.time_since_last_command_ < 24 ) {
            // Draw the black background.
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();
            upper_left.y = upper_left.y - 10;

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 10;

            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Black, true);

            //Overlay the appropriate grey above it.
            lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width() * ( 1 - min(unit.time_since_last_command_, 24) / (double)24 );
            lower_right.y = upper_left.y + 10;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Grey, true);

        }
    }
}

// Outlines the case where UNIT cannot attack ENEMY type (air/ground), while ENEMY can attack UNIT.  Essentially bidirectional Can_Fight checks.
bool CUNYAIModule::Futile_Fight( Unit unit, Unit enemy ) {
    bool e_invunerable = (enemy->isFlying() && unit->getType().airWeapon() == WeaponTypes::None ) || (!enemy->isFlying() && unit->getType().groundWeapon() == WeaponTypes::None) || unit->getType() == UnitTypes::Terran_Bunker || unit->getType() == UnitTypes::Protoss_Carrier || (unit->getType() == UnitTypes::Protoss_Reaver && !enemy->isFlying()); // if we cannot attack them.
    bool u_vunerable = (unit->isFlying() && enemy->getType().airWeapon() != WeaponTypes::None) || (!unit->isFlying() && enemy->getType().groundWeapon() != WeaponTypes::None) || enemy->getType() == UnitTypes::Terran_Bunker || enemy->getType() == UnitTypes::Protoss_Carrier || (enemy->getType() == UnitTypes::Protoss_Reaver && !unit->isFlying()); // they can attack us.
    
    return ( e_invunerable && u_vunerable ) || ( u_vunerable && !enemy->isDetected() ); // also if they are cloaked and can attack us.
}

// Outlines the case where UNIT can attack ENEMY;
bool CUNYAIModule::Can_Fight( Unit unit, Unit enemy ) {
    UnitType e_type = enemy->getType();
    UnitType u_type = unit->getType();
    bool has_appropriate_weapons = (e_type.isFlyer() && u_type.airWeapon() != WeaponTypes::None) || (!e_type.isFlyer() && u_type.groundWeapon() != WeaponTypes::None);
    bool is_critical_type = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier || u_type == UnitTypes::Protoss_Reaver;
    bool e_vunerable = (has_appropriate_weapons || is_critical_type); // if we cannot attack them.
    if ( enemy->exists() ) {
        return e_vunerable && enemy->isDetected();
    }
    else {
        return e_vunerable; // also if they are cloaked and can attack us.
    }
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight( Unit unit, Stored_Unit enemy ) {
    UnitType e_type = enemy.type_;
    UnitType u_type = unit->getType();
    bool has_appropriate_weapons = (e_type.isFlyer() && u_type.airWeapon() != WeaponTypes::None) || (!e_type.isFlyer() && u_type.groundWeapon() != WeaponTypes::None);
    bool is_critical_type = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier || u_type == UnitTypes::Protoss_Reaver;
    bool e_vunerable = (has_appropriate_weapons || is_critical_type); // if we cannot attack them.
    if (enemy.bwapi_unit_ && enemy.bwapi_unit_->exists()) {
        return e_vunerable && enemy.bwapi_unit_->isDetected();
    }
    else {
        return e_vunerable; // also if they are cloaked and can attack us.
    }
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight(Stored_Unit unit, Stored_Unit enemy) {
    UnitType e_type = enemy.type_;
    UnitType u_type = unit.type_;
    bool has_appropriate_weapons = (e_type.isFlyer() && u_type.airWeapon() != WeaponTypes::None) || (!e_type.isFlyer() && u_type.groundWeapon() != WeaponTypes::None);
    bool is_critical_type = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier || u_type == UnitTypes::Protoss_Reaver;
    bool e_vunerable = (has_appropriate_weapons || is_critical_type); // if we cannot attack them.
    if ( enemy.bwapi_unit_ && enemy.bwapi_unit_->exists() ) {
        return e_vunerable && enemy.bwapi_unit_->isDetected();
    }
    else {
        return e_vunerable; // also if they are cloaked and can attack us.
    }
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight( Stored_Unit unit, Unit enemy ) {
    UnitType e_type = enemy->getType();
    UnitType u_type = unit.type_;
    bool has_appropriate_weapons = (e_type.isFlyer() && u_type.airWeapon() != WeaponTypes::None) || (!e_type.isFlyer() && u_type.groundWeapon() != WeaponTypes::None);
    bool is_critical_type = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier || u_type == UnitTypes::Protoss_Reaver;
    bool e_vunerable = (has_appropriate_weapons || is_critical_type); // if we cannot attack them.
    if (enemy->exists()) {
        return e_vunerable && enemy->isDetected();
    }
    else {
        return e_vunerable; // also if they are cloaked and can attack us.
    }
}

bool CUNYAIModule::Can_Fight_Type(UnitType unittype, UnitType enemytype)
{
    bool has_appropriate_weapons = (enemytype.isFlyer() && unittype.airWeapon() != WeaponTypes::None) || (!enemytype.isFlyer() && unittype.groundWeapon() != WeaponTypes::None);
    bool is_critical_type = unittype == UnitTypes::Terran_Bunker || unittype == UnitTypes::Protoss_Carrier || unittype == UnitTypes::Protoss_Reaver;
    bool e_vunerable = (has_appropriate_weapons || is_critical_type); // if we cannot attack them.

    return e_vunerable; // also if they are cloaked and can attack us.

}

// Counts all units of one type in existance and owned by enemies. Counts units under construction.
int CUNYAIModule::Count_Units( const UnitType &type, const Unit_Inventory &ui )
{
    int count = 0;

    for ( auto & e : ui.unit_inventory_) {

        //if ( e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type ) { // Count units under construction
        //    count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        //} 
        //else if ( e.second.type_ == type ) {
        //    count++;
        //}

        count += (e.second.type_ == type) + !(e.second.type_ == type) * 2 * (e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type); // better without if-conditions.
    }

    return count;
}

// Overload. (Very slow, since it uses BWAPI Unitsets) Counts all units in a set of one type owned by player. Includes individual units in production. 
int CUNYAIModule::Count_Units( const UnitType &type, const Unitset &unit_set )
{
    int count = 0;
    for ( auto & unit : unit_set )
    {
        //if ( unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type ) { // Count units under construction
        //    count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        //} 
        //else if ( unit->getType() == type ) {
        //    count++;
        //}
        count += (unit->getType() == type) + !(unit->getType() == type) * 2 * (unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type); // better without if-conditions.

    }

    return count;
}

// Overload. Counts all units in a set of one type in the reservation system. Does not reserve larva units. 
int CUNYAIModule::Count_Units( const UnitType &type, const Reservation &res )
{
    int count = 0;
    map<UnitType, TilePosition>::const_iterator it = res.reservation_map_.find( type );
    if ( it != res.reservation_map_.end() ) {
        count++;
    }

    return count;
}

// Counts all units of one type in existance and owned by me. Counts units under construction.
int CUNYAIModule::Count_Units(const UnitType &type, const Inventory &inv)
{
    auto c_iter = find(inv.unit_type_.begin(), inv.unit_type_.end(), type);
    if (c_iter == inv.unit_type_.end()) {
        return 0;
    }
    else {
        int distance = std::distance(inv.unit_type_.begin(), c_iter);
        return inv.unit_count_[distance];
    }

}
// Counts all units of one type in existance and in progress by me. Counts units under construction.
int CUNYAIModule::Count_Units_In_Progress(const UnitType &type, const Inventory &inv)
{
    auto c_iter = find(inv.unit_type_.begin(), inv.unit_type_.end(), type);
    if (c_iter == inv.unit_type_.end()) {
        return 0;
    }
    else {
        int distance = std::distance(inv.unit_type_.begin(), c_iter);
        return inv.unit_incomplete_[distance];
    }
}

// Overload. Counts all units in a set of one type owned by player. Includes individual units in production. 
int CUNYAIModule::Count_Units_Doing(const UnitType &type, const UnitCommandType &u_command_type, const Unitset &unit_set)
{
	int count = 0;
	for (const auto & unit : unit_set)
	{
		if (unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type) { // Count units under construction
			count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
		}
		else if (unit->getType() == type && unit->getLastCommand().getType() == u_command_type) {
			count++;
		}
	}

	return count;
}
// Overload. Counts all units in a set of one type owned by player. Includes individual units in production. 
int CUNYAIModule::Count_Units_Doing(const UnitType &type, const UnitCommandType &u_command_type, const Unit_Inventory &ui)
{
    int count = 0;
    for (const auto & unit : ui.unit_inventory_)
    {
        if (unit.second.type_ == UnitTypes::Zerg_Egg && unit.second.build_type_ == type) { // Count units under construction
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        }
        else if (unit.second.type_ == UnitTypes::Zerg_Drone && unit.second.build_type_ == type) { // Count units under construction
            count++;
        }
        else if (unit.second.type_ == type && unit.second.bwapi_unit_ && unit.second.bwapi_unit_->exists() && unit.second.bwapi_unit_->getLastCommand().getType() == u_command_type) {
            count++;
        }
    }

    return count;
}

// Overload. Counts all units in a set of one type owned by player. Includes individual units in production.  I have doubts about this function.
int CUNYAIModule::Count_Units_In_Progress(const UnitType &type, const Unit_Inventory &ui)
{
    int count = 0;
    for (const auto & unit : ui.unit_inventory_)
    {
        if (unit.second.type_ == UnitTypes::Zerg_Egg && unit.second.build_type_ == type) { // Count units under construction
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        }
        else if (unit.second.type_ == UnitTypes::Zerg_Drone && unit.second.build_type_ == type) { // Count units under construction
            count++;
        }
        else if (unit.second.bwapi_unit_ && unit.second.bwapi_unit_->getBuildType() == type ) { // Count units under construction
            count++;
        }

    }

    return count;
}

// evaluates the value of a stock of buildings, in terms of pythagorian distance of min & gas & supply. Assumes building is zerg and therefore, a drone was spent on it.
int CUNYAIModule::Stock_Buildings( const UnitType &building, const Unit_Inventory &ui ) {
    int cost = building.mineralPrice() + UnitTypes::Zerg_Drone.mineralPrice() + 1.25 * building.gasPrice() + UnitTypes::Zerg_Drone.gasPrice() + 25 * UnitTypes::Zerg_Drone.supplyRequired();
    int instances = Count_Units( building , ui );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of pythagorian distance of min & gas & supply. Counts totals of stacked upgrades like melee/range/armor.
int CUNYAIModule::Stock_Ups( const UpgradeType &ups ) {
    int lvl = Broodwar->self()->getUpgradeLevel( ups ) + (int)Broodwar->self()->isUpgrading( ups );
    int total_stock = 0;
    for ( int i = 1; i <= lvl; i++ ) {
        int cost = ups.mineralPrice() + 1.25 * ups.gasPrice();
        total_stock += cost;
    }
    return total_stock;
}

int CUNYAIModule::Stock_Tech(const TechType &tech) {
    bool lvl = Broodwar->self()->hasResearched(tech) + (int)Broodwar->self()->isResearching(tech);
    int total_stock = 0;
    if ( lvl ) {
        int cost = tech.mineralPrice() + 1.25 * tech.gasPrice();
        total_stock += cost;
    }
    return total_stock;
}

int CUNYAIModule::Stock_Units( const UnitType &unit_type, const Unit_Inventory &ui) {
    int total_stock = 0;

    for ( auto & u : ui.unit_inventory_ ) {
        if ( u.second.type_ == unit_type ) {  // if you impose valid_pos here many of YOUR OWN UNITS will not be counted.
            total_stock += u.second.current_stock_value_;
        }
    }

    return total_stock;
}

// evaluates the value of a stock of combat units, for all unit types in a unit inventory. Does not count eggs.
int CUNYAIModule::Stock_Combat_Units( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
        if ( ((UnitType)i).airWeapon() != WeaponTypes::None || ((UnitType)i).groundWeapon() != WeaponTypes::None || ((UnitType)i).maxEnergy() > 0 ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// Overload. evaluates the value of a stock of units, for all unit types in a unit inventory
int CUNYAIModule::Stock_Units_ShootUp( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
        if ( ((UnitType)i).airWeapon() != WeaponTypes::None ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// Overload. evaluates the value of a stock of allied units, for all unit types in a unit inventory
int CUNYAIModule::Stock_Units_ShootDown( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
        if ( ((UnitType)i).groundWeapon() != WeaponTypes::None ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int CUNYAIModule::Stock_Supply( const UnitType &unit, const Inventory &inv ) {
    int supply = unit.supplyProvided();
    int instances = Count_Units( unit, inv );
    int total_stock = supply * instances;
    return total_stock;
}

// returns helpful_friendly and helpful_enemy units from respective inventories.
vector<int> CUNYAIModule::getUsefulStocks(const Unit_Inventory & friend_loc, const Unit_Inventory & enemy_loc)
{
    int helpful_e, helpful_u;

        //helpful_e = min(enemy_loc.stock_shoots_down_, friend_loc.stock_ground_units_ * 2) + min(enemy_loc.stock_shoots_up_, friend_loc.stock_fliers_ * 2) - min(min(enemy_loc.stock_both_up_and_down_, friend_loc.stock_fliers_ * 2), friend_loc.stock_ground_units_ * 2); // A+B - A
        //helpful_u = min(friend_loc.stock_shoots_down_, enemy_loc.stock_ground_units_ * 2) + min(friend_loc.stock_shoots_up_, enemy_loc.stock_fliers_ * 2) - min(min(friend_loc.stock_both_up_and_down_, enemy_loc.stock_fliers_ * 2), enemy_loc.stock_ground_units_ * 2); // A+B - A


        helpful_e = enemy_loc.stock_shoots_down_ * (friend_loc.stock_ground_units_ > 0) + enemy_loc.stock_shoots_up_ * (friend_loc.stock_fliers_ > 0) - enemy_loc.stock_both_up_and_down_ * (friend_loc.stock_fliers_ > 0) * (friend_loc.stock_ground_units_ > 0); // A+B - A Union B
           //if (friend_loc.stock_ground_units_ == 0) {
            //    helpful_e = enemy_loc.stock_shoots_up_;
            //}
            //else if (friend_loc.stock_fliers_ == 0) {
            //    helpful_e = enemy_loc.stock_shoots_down_;
            //}
        helpful_u = friend_loc.stock_shoots_down_ * (enemy_loc.stock_ground_units_ > 0) + friend_loc.stock_shoots_up_ * (enemy_loc.stock_fliers_ > 0) - friend_loc.stock_both_up_and_down_ * (enemy_loc.stock_fliers_ > 0) * (enemy_loc.stock_ground_units_ > 0); // A+B - A
            //if (enemy_loc.stock_ground_units_ == 0) {
            //    helpful_u = friend_loc.stock_shoots_up_;
            //}
            //else if (enemy_loc.stock_fliers_ == 0) {
            //    helpful_u = friend_loc.stock_shoots_down_;
            //}
            
        vector<int> return_vec = { helpful_u, helpful_e };
        return return_vec;
}
int CUNYAIModule::getTargetableStocks(const Unit & u, const Unit_Inventory & enemy_loc)
{
    int targetable_e;
    targetable_e = (u->getType().airWeapon() != WeaponTypes::None) * (enemy_loc.stock_fliers_ + enemy_loc.stock_air_fodder_ ) + (u->getType().groundWeapon() != WeaponTypes::None) * (enemy_loc.stock_ground_units_ + enemy_loc.stock_ground_fodder_);
    return targetable_e;
}

int CUNYAIModule::getThreateningStocks(const Unit & u, const Unit_Inventory & enemy_loc)
{
    int threatening_e;
    threatening_e = u->getType().isFlyer() * enemy_loc.stock_shoots_up_  +  !u->getType().isFlyer() * enemy_loc.stock_shoots_down_;
    return threatening_e;
}

// Announces to player the name and type of all units in the unit inventory. Bland but practical.
void CUNYAIModule::Print_Unit_Inventory( const int &screen_x, const int &screen_y, const Unit_Inventory &ui ) {
    int another_sort_of_unit = 0;
    for ( int i = 0; i != 229; i++ )
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        int u_count = Count_Units( ((UnitType)i), ui );
        if ( u_count > 0 ) {
            Broodwar->drawTextScreen( screen_x, screen_y, "Inventoried Units:" );  //
            Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10 , "%s: %d", noRaceName( ((UnitType)i).c_str()), u_count );  //
            another_sort_of_unit++;
        }
    }
}

// Announces to player the name and type of all units in the unit inventory. Bland but practical.
void CUNYAIModule::Print_Universal_Inventory(const int &screen_x, const int &screen_y, const Inventory &inv) {
    int another_sort_of_unit = 0;
    for (auto i : inv.unit_type_)
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        int u_count = CUNYAIModule::Count_Units(i, inv);
        int u_incomplete_count = CUNYAIModule::Count_Units_In_Progress(i, inv);
        if (u_count > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Inventoried Units:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s: %d Inc: %d", noRaceName( i.c_str() ), u_count, u_incomplete_count);  //
            another_sort_of_unit++;
        }
    }
}

// Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
void CUNYAIModule::Print_Build_Order_Remaining( const int &screen_x, const int &screen_y, const Building_Gene &bo ) {
    int another_sort_of_unit = 0;
    if ( !bo.building_gene_.empty() ) {
        for ( auto i : bo.building_gene_ ) { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            Broodwar->drawTextScreen( screen_x, screen_y, "Build Order:" );  //
            if ( i.getUnit() != UnitTypes::None ) {
                Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s", noRaceName( i.getUnit().c_str() ) );  //
            }
            else if ( i.getUpgrade() != UpgradeTypes::None ) {
                Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s", i.getUpgrade().c_str() );  //
            }
            another_sort_of_unit++;
        }
    }
    else {
        Broodwar->drawTextScreen( screen_x, screen_y, "Build Order:" );  //
        Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "Build Order Empty");  //
    }
}

// Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
void CUNYAIModule::Print_Upgrade_Inventory( const int &screen_x, const int &screen_y ) {
    int another_sort_of_upgrade = 0;
    for ( int i = 0; i != 62; i++ )
    { // iterating through all upgrades.
        int up_count = Broodwar->self()->getUpgradeLevel( ((UpgradeType)i) ) + (int)Broodwar->self()->isUpgrading( ((UpgradeType)i) );
        if ( up_count > 0 ) {
            Broodwar->drawTextScreen( screen_x, screen_y, "Upgrades:" );  //
            Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: %d", ((UpgradeType)i).c_str() , up_count );  //
            another_sort_of_upgrade++;
        }
    }
    if ( Broodwar->self()->hasResearched( TechTypes::Lurker_Aspect )) {
        Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: 1", TechTypes::Lurker_Aspect.c_str(), 1 );  //
    }
}

// Announces to player the name and type of all buildings in the reservation system. Bland but practical.
void CUNYAIModule::Print_Reservations( const int &screen_x, const int &screen_y, const Reservation &res ) {
    int another_sort_of_unit = 0;
    for ( int i = 0; i != 229; i++ )
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        int u_count = Count_Units( ((UnitType)i), res );
        if ( u_count > 0 ) {
            Broodwar->drawTextScreen( screen_x, screen_y, "Reserved Buildings:" );  //
            Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s: %d", noRaceName( ((UnitType)i).c_str() ), u_count );  //
            another_sort_of_unit++;
        }
    }
}

//Strips the RACE_ from the front of the unit type string.
const char * CUNYAIModule::noRaceName( const char *name ) { //From N00b
    for ( const char *c = name; *c; c++ )
        if ( *c == '_' ) return ++c;
    return name;
}

//Converts a unit inventory into a unit set directly. Checks range. Careful about visiblity.
Unitset CUNYAIModule::getUnit_Set( const Unit_Inventory &ui, const Position &origin, const int &dist ) {
    Unitset e_set;
    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if ( (*e).second.pos_.getDistance( origin ) <= dist ) {
            e_set.insert( (*e).second.bwapi_unit_ ); // if we take any distance and they are in inventory.
        }
    }
    return e_set;
}

//Gets pointer to closest unit to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStored( Unit_Inventory &ui, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if ( !ui.unit_inventory_.empty() ) {
        for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
            temp_dist = (*e).second.pos_.getDistance( origin );
            if ( temp_dist <= min_dist && e->second.valid_pos_ ) {
                min_dist = temp_dist;
                return_unit = &(e->second);
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit of a type to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStored(Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999) {
	int min_dist = dist;
	int temp_dist = 999999;
	Stored_Unit* return_unit = nullptr;

	if (!ui.unit_inventory_.empty()) {
		for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
			if (e->second.type_ == u_type && e->second.valid_pos_ ){
				temp_dist = (*e).second.pos_.getDistance(origin);
				if (temp_dist <= min_dist) {
					min_dist = temp_dist;
					return_unit = &(e->second);
				}
			}
		}
	}

	return return_unit;
}

//Gets pointer to closest unit to point in Resource_inventory. Checks range. Careful about visiblity.
Stored_Resource* CUNYAIModule::getClosestStored(Resource_Inventory &ri, const Position &origin, const int &dist = 999999) {
	int min_dist = dist;
	int temp_dist = 999999;
	Stored_Resource* return_unit = nullptr;

	if (!ri.resource_inventory_.empty()) {
		for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
			temp_dist = (*r).second.pos_.getDistance(origin);
			if (temp_dist <= min_dist ) {
				min_dist = temp_dist;
				return_unit = &(r->second);
			}
		}
	}

	return return_unit;
}


Stored_Resource* CUNYAIModule::getClosestGroundStored(Resource_Inventory &ri, Inventory &inv, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            temp_dist = inv.getDifferentialDistanceOutFromHome(r->second.pos_, origin); // can't be const because of this line.
            if (temp_dist <= min_dist) {
                min_dist = temp_dist;
                return_unit = &(r->second);
            }
        }
    }

    return return_unit;
}
// Allows type -specific- selection. 
Stored_Resource* CUNYAIModule::getClosestGroundStored(Resource_Inventory &ri,const UnitType type, Inventory &inv, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            temp_dist = inv.getDifferentialDistanceOutFromHome(r->second.pos_, origin); // can't be const because of this line.
            bool right_type = (type == r->second.type_ || type.isMineralField() && r->second.type_.isMineralField()); //WARNING:: Minerals have 4 types.
            if (temp_dist <= min_dist && right_type ) { 
                min_dist = temp_dist;
                return_unit = &(r->second);
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStoredBuilding(Unit_Inventory &ui, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_inventory_.empty()) {
        for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
            if (e->second.type_.isBuilding()) {
                temp_dist = (*e).second.pos_.getDistance(origin);
                if (temp_dist <= min_dist && e->second.valid_pos_) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit to point in Resource_inventory. Checks range. Careful about visiblity.
Stored_Resource* CUNYAIModule::getClosestStored(Resource_Inventory &ri, const UnitType &r_type, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            if (r->second.type_ == r_type && r->second.valid_pos_) {
                temp_dist = (int)(*r).second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(r->second);
                }
            }
        }
    }

    return return_unit;
}


//Gets pointer to closest attackable unit from unit in Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr.
Stored_Unit* CUNYAIModule::getClosestAttackableStored(Unit_Inventory &ui, const Unit unit, const int &dist = 999999) {
    int min_dist = dist;
    bool can_attack;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_inventory_.empty()) {
        for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
            can_attack = CUNYAIModule::Can_Fight(unit, e->second);
            if (can_attack && e->second.pos_.isValid() && e->second.valid_pos_) {
                temp_dist = (int)e->second.pos_.getDistance(unit->getPosition());
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getClosestThreatOrTargetStored( Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if ( !ui.unit_inventory_.empty() ) {
        for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
            can_attack = Can_Fight_Type(u_type, e->second.type_) && e->second.bwapi_unit_;
            can_be_attacked_by = Can_Fight_Type(e->second.type_, u_type);
            if ( (can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                temp_dist = (int)e->second.pos_.getDistance( origin );
                if ( temp_dist <= min_dist ) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getClosestThreatOrTargetStored(Unit_Inventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_inventory_.empty()) {
        for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
            can_attack = Can_Fight(unit, e->second);
            can_be_attacked_by = Can_Fight(e->second, unit);

            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                temp_dist = (int)e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}
//Gets pointer to closest threat/target unit from home within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getMostAdvancedThreatOrTargetStored(Unit_Inventory &ui, const Unit &unit, const Inventory &inv, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by, we_are_a_flyer;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;
    Position origin = unit->getPosition();
    we_are_a_flyer = unit->getType().isFlyer();

    if (!ui.unit_inventory_.empty()) {
        for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
            can_attack = Can_Fight(unit, e->second);
            can_be_attacked_by = Can_Fight(e->second, unit);
            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                if (we_are_a_flyer) {
                    temp_dist = unit->getDistance(e->second.pos_);
                }
                else {
                    temp_dist = inv.getRadialDistanceOutFromHome(e->second.pos_);
                }

                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitInventoryInRadius( const Unit_Inventory &ui, const Position &origin, const int &dist ) {
    Unit_Inventory ui_out;
    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if ( (*e).second.pos_.getDistance( origin ) <= dist && e->second.valid_pos_) {
            ui_out.addStored_Unit( (*e).second ); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr. Overloaded for specifi types.
Unit_Inventory CUNYAIModule::getUnitInventoryInRadius(const Unit_Inventory &ui, const UnitType u_type, const Position &origin, const int &dist) {
	Unit_Inventory ui_out;
	for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
		if ((*e).second.pos_.getDistance(origin) <= dist && (*e).second.type_== u_type && e->second.valid_pos_ ) {
			ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
		}
	}
	return ui_out;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
Resource_Inventory CUNYAIModule::getResourceInventoryInRadius(const Resource_Inventory &ri, const Position &origin, const int &dist) {
	Resource_Inventory ri_out;
	for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
		if ((*r).second.pos_.getDistance(origin) <= dist) {
			ri_out.addStored_Resource( (*r).second ); // if we take any distance and they are in inventory.
		}
	}
	return ri_out;
}

//Searches an enemy inventory for units within a range. Returns units that are not in weapon range but are in inventory. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitsOutOfReach(const Unit_Inventory &ui, const Unit &target) {
    Unit_Inventory ui_out;
    for (auto & u = ui.unit_inventory_.begin(); u != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); u++) {
        if (u->second.valid_pos_ && ( !(*u).second.bwapi_unit_->canMove() && !(*u).second.bwapi_unit_->isInWeaponRange(target) ) ) {
            ui_out.addStored_Unit((*u).second); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an inventory for units of within a range. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
bool CUNYAIModule::checkOccupiedArea( const Unit_Inventory &ui, const Position &origin, const int &dist ) {

    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if( (*e).second.pos_.x < origin.x + dist && (*e).second.pos_.x > origin.x - dist &&
            (*e).second.pos_.y < origin.y + dist && (*e).second.pos_.y > origin.y - dist ) {
            return true;
        }
    }

    return false;
}

//Searches an inventory for buildings. Returns TRUE if the area is occupied. 
bool CUNYAIModule::checkOccupiedArea(const Unit_Inventory &ui, const UnitType type, const Position &origin) {

    for (auto & e : ui.unit_inventory_) {
        if (e.second.type_ == type) {
            if (e.second.pos_.x < origin.x + e.second.type_.dimensionLeft() && e.second.pos_.x > origin.x - e.second.type_.dimensionRight() &&
                e.second.pos_.y < origin.y + e.second.type_.dimensionUp() && e.second.pos_.y > origin.y - e.second.type_.dimensionDown()) {
                return true;
            }
        }
    }

    return false;
}

//Searches an inventory for units of within a range. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
//bool CUNYAIModule::checkThreatenedArea(const Unit_Inventory &ui, const UnitType &type, const Position &origin, const int &dist) {
//
//    for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
//        if ((*e).second.pos_.x < origin.x + dist && (*e).second.pos_.x > origin.x - dist &&
//            (*e).second.pos_.y < origin.y + dist && (*e).second.pos_.y > origin.y - dist &&
//            CUNYAIModule::Can_Fight(e->second.type_, Stored_Unit(type) ) ) {
//            return true;
//        }
//    }
//
//    return false;
//}

//Searches an inventory for buildings. Returns TRUE if the area is occupied. 
bool CUNYAIModule::checkBuildingOccupiedArea( const Unit_Inventory &ui, const Position &origin ) {

    for ( auto & e : ui.unit_inventory_) {
        if ( e.second.type_.isBuilding() ) {
            if ( e.second.pos_.x < origin.x + e.second.type_.dimensionLeft() && e.second.pos_.x > origin.x - e.second.type_.dimensionRight() &&
                e.second.pos_.y < origin.y + e.second.type_.dimensionUp() && e.second.pos_.y > origin.y - e.second.type_.dimensionDown() ) {
                return true;
            }
        }
    }

    return false;
}

//Searches an inventory for a resource
bool CUNYAIModule::checkResourceOccupiedArea( const Resource_Inventory &ri, const Position &origin ) {

    for ( auto & e : ri.resource_inventory_ ) {
        if ( e.second.pos_.x < origin.x + e.second.type_.dimensionLeft() && e.second.pos_.x > origin.x - e.second.type_.dimensionRight() &&
            e.second.pos_.y < origin.y + e.second.type_.dimensionUp() && e.second.pos_.y > origin.y - e.second.type_.dimensionDown() ) {
            return true;
        }
    }

    return false;
}

//Searches if a particular unit is within a range of the position. Returns TRUE if the area is occupied or nearly so. Checks retangles for performance reasons rather than radius.
bool CUNYAIModule::checkUnitOccupiesArea( const Unit &unit, const Position &origin, const int & dist ) {

        if ( unit->getType().isBuilding() ) {
            Position pos = unit->getPosition();
            UnitType type = unit->getType();
            if ( pos.x < origin.x + type.dimensionLeft() + dist && pos.x > origin.x - type.dimensionRight() - dist &&
                pos.y < origin.y + type.dimensionUp() + dist && pos.y > origin.y - type.dimensionDown() - dist ) {
                return true;
            }
        }


    return false;
}

bool CUNYAIModule::isOnScreen( const Position &pos , const Position &screen_pos) {
    bool inrange_x = screen_pos.x < pos.x && screen_pos.x + 640 > pos.x;
    bool inrange_y = screen_pos.y < pos.y && screen_pos.y + 480 > pos.y;
    return inrange_x && inrange_y;
}

//Returns TRUE if the unit is ready to move and false if the unit should be ignored for now.
bool CUNYAIModule::spamGuard(const Unit &unit, int cd_frames_chosen) {

    bool ready_to_move = true;
    bool wait_for_cooldown = false;
    int cd_frames = 0;

    if (cd_frames_chosen == 99) {
        cd_frames = 0;
    } 
    else { // if the person has selected some specific delay they are looking for, check that.
        ready_to_move = unit->getLastCommandFrame() < Broodwar->getFrameCount() - cd_frames_chosen;
        return ready_to_move;
    }

    bool unit_fighting = unit->isStartingAttack();
    if (unit_fighting) {
        ready_to_move = false;
        return ready_to_move;
    }

    UnitCommandType u_command = unit->getLastCommand().getType();

    //if ( u_command == UnitCommandTypes::Attack_Unit || u_command == UnitCommandTypes::Attack_Move ) {
    //    UnitType u_type = unit->getType();
    //    //cd_frames = Broodwar->getLatencyFrames();
    //    //if (u_type == UnitTypes::Zerg_Drone) {
    //    //    cd_frames = 1;
    //    //}
    //    //else if (u_type == UnitTypes::Zerg_Zergling) {
    //    //    cd_frames = 5;
    //    //}
    //    //else if (u_type == UnitTypes::Zerg_Hydralisk) {
    //    //    cd_frames = 7;
    //    //}
    //    //else if (u_type == UnitTypes::Zerg_Lurker) {
    //    //    cd_frames = 2;
    //    //}
    //    //else if (u_type == UnitTypes::Zerg_Mutalisk) {
    //    //    cd_frames = 1;
    //    //}
    //    //else if (u_type == UnitTypes::Zerg_Ultralisk) {
    //    //    cd_frames = 15;
    //    //}
    //    //wait_for_cooldown = unit->getGroundWeaponCooldown() > 0 || unit->getAirWeaponCooldown() > 0;
    //}
    //else 
    if (u_command == UnitCommandTypes::Burrow || u_command == UnitCommandTypes::Unburrow) {
        cd_frames = 14;
    }

    if (u_command == UnitCommandTypes::Morph) {
        cd_frames = 14;
    }
    //if (u_command == UnitCommandTypes::Attack_Move) {
    //    cd_frames += 2; // an ad-hoc delay for aquiring targets, I don't know what it is formally atm.
    //}
    //if (u_order == Orders::Move && ( (!unit->isMoving() && !unit->isAccelerating()) || unit->isBraking() ) ) {
    //    cd_frames = 7; // if it's not moving, accellerating or IS breaking.
    //}

    //if ( u_order == Orders::AttackMove) {
    //    cd_frames = 12;
    //}

    //if (cd_frames < Broodwar->getLatencyFrames() ) {
    //    cd_frames = Broodwar->getLatencyFrames();
    //}

    ready_to_move = Broodwar->getFrameCount() - unit->getLastCommandFrame() > cd_frames;
    return ready_to_move; // we must wait at least 5 frames before issuing them a new command regardless.

}

//checks if there is a smooth path to target. in minitiles
//bool CUNYAIModule::isClearRayTrace( const Position &initial, const Position &final, const Inventory &inv ) // see Brehsam's Algorithm
//{
//    int dx = abs( final.x - initial.x ) / 8;
//    int dy = abs( final.y - initial.y ) / 8;
//    int x = initial.x / 8;
//    int y = initial.y / 8;
//    int n = 1 + dx + dy;
//    int x_inc = (final.x > initial.x) ? 1 : -1;
//    int y_inc = (final.y > initial.y) ? 1 : -1;
//    int error = dx - dy;
//    dx *= 2;
//    dy *= 2;
//
//    for ( ; n > 0; --n )
//    {
//        if ( inv.smoothed_barriers_[x][y] == 1 ) {
//            return false;
//        }
//
//        if ( error > 0 )
//        {
//            x += x_inc;
//            error -= dy;
//        }
//        else
//        {
//            y += y_inc;
//            error += dx;
//        }
//    }
//
//    return true;
//} 

//checks if there is a smooth path to target. in minitiles
bool CUNYAIModule::isClearRayTrace(const Position &initialp, const Position &finalp, const Inventory &inv){ // see Brehsam's Algorithm for all 8 octants.
	int x, y, dx, dy, dx1, dy1, px, py, xe, ye, map_x, map_y;
	WalkPosition final = WalkPosition(finalp);
	WalkPosition initial = WalkPosition(initialp);

	dx = (final.x - initial.x);
	dy = (final.y - initial.y);
	dx1 = abs(dx);
	dy1 = abs(dy);
	px = 2 * dy1 - dx1;
	py = 2 * dx1 - dy1;
	map_x = Broodwar->mapWidth() * 4;
	map_y = Broodwar->mapHeight() * 4;

	if (dy1 <= dx1)
	{
		if (dx >= 0)
		{
			x = initial.x;
			y = initial.y;
			xe = final.x;
		}
		else
		{
			x = final.x;
			y = final.y;
			xe = initial.x;
		}

		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y ;
		if ( safety_check && inv.map_veins_[x][y] == 1) {
			return false;
		}

		for (int i = 0; x<xe; i++)
		{
			x = x + 1;
			if (px<0)
			{
				px = px + 2 * dy1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					y++;
				}
				else
				{
					y--;
				}
				px = px + 2 * (dy1 - dx1);
			}

			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if (safety_check && inv.map_veins_[x][y] == 1) {
				return false;
			}
		}
	}
	else
	{
		if (dy >= 0)
		{
			x = initial.x;
			y = initial.y;
			ye = final.y;
		}
		else
		{
			x = final.x;
			y = final.y;
			ye = initial.y;
		}
		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
		if (safety_check && inv.map_veins_[x][y] == 1) {
			return false;
		}

		for (int i = 0; y<ye; i++)
		{
			y = y + 1;
			if (py <= 0)
			{
				py = py + 2 * dx1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					x++;
				}
				else
				{
					x--;
				}
				py = py + 2 * (dx1 - dy1);
			}
			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if (safety_check && inv.map_veins_[x][y] == 1) {
				return false;
			}

		}
	}

	return true;

}

//checks if there is a smooth path to target. in minitiles
bool CUNYAIModule::isMapClearRayTrace( const Position &initialp, const Position &finalp, const Inventory &inv ) { // see Brehsam's Algorithm for all 8 octants.
    int x, y, dx, dy, dx1, dy1, px, py, xe, ye, map_x, map_y;
    WalkPosition final = WalkPosition( finalp );
    WalkPosition initial = WalkPosition( initialp );

    dx = (final.x - initial.x);
    dy = (final.y - initial.y);
    dx1 = abs( dx );
    dy1 = abs( dy );
    px = 2 * dy1 - dx1;
    py = 2 * dx1 - dy1;
    map_x = Broodwar->mapWidth() * 4;
    map_y = Broodwar->mapHeight() * 4;

    if ( dy1 <= dx1 )
    {
        if ( dx >= 0 )
        {
            x = initial.x;
            y = initial.y;
            xe = final.x;
        }
        else
        {
            x = final.x;
            y = final.y;
            xe = initial.x;
        }

        bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
        if ( safety_check && inv.unwalkable_barriers_[x][y] == 1 ) {
            return false;
        }

        for ( int i = 0; x<xe; i++ )
        {
            x = x + 1;
            if ( px<0 )
            {
                px = px + 2 * dy1;
            }
            else
            {
                if ( (dx<0 && dy<0) || (dx>0 && dy>0) )
                {
                    y++;
                }
                else
                {
                    y--;
                }
                px = px + 2 * (dy1 - dx1);
            }

            bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
            if ( safety_check && inv.unwalkable_barriers_[x][y] == 1 ) {
                return false;
            }
        }
    }
    else
    {
        if ( dy >= 0 )
        {
            x = initial.x;
            y = initial.y;
            ye = final.y;
        }
        else
        {
            x = final.x;
            y = final.y;
            ye = initial.y;
        }
        bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
        if ( safety_check && inv.unwalkable_barriers_[x][y] == 1 ) {
            return false;
        }

        for ( int i = 0; y<ye; i++ )
        {
            y = y + 1;
            if ( py <= 0 )
            {
                py = py + 2 * dx1;
            }
            else
            {
                if ( (dx<0 && dy<0) || (dx>0 && dy>0) )
                {
                    x++;
                }
                else
                {
                    x--;
                }
                py = py + 2 * (dx1 - dy1);
            }
            bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
            if ( safety_check && inv.unwalkable_barriers_[x][y] == 1 ) {
                return false;
            }

        }
    }

    return true;

}

//Counts the number of minitiles in a smooth path to target. in minitiles
int CUNYAIModule::getClearRayTraceSquares( const Position &initialp, const Position &finalp, const Inventory &inv ) // see Brehsam's Algorithm. Is likely bugged in current state.
{
	int x, y, dx, dy, dx1, dy1, px, py, xe, ye, map_x, map_y, squares_counted;
	WalkPosition final = WalkPosition(finalp);
	WalkPosition initial = WalkPosition(initialp);

	squares_counted = 0;
	dx = (final.x - initial.x);
	dy = (final.y - initial.y);
	dx1 = abs(dx);
	dy1 = abs(dy);
	px = 2 * dy1 - dx1;
	py = 2 * dx1 - dy1;
	map_x = Broodwar->mapWidth() * 4;
	map_y = Broodwar->mapHeight() * 4;

	if (dy1 <= dx1)
	{
		if (dx >= 0)
		{
			x = initial.x;
			y = initial.y;
			xe = final.x;
		}
		else
		{
			x = final.x;
			y = final.y;
			xe = initial.x;
		}

		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
		if (safety_check && inv.map_veins_[x][y] == 1) {
			squares_counted++;
		}

		for (int i = 0; x<xe; i++)
		{
			x = x + 1;
			if (px<0)
			{
				px = px + 2 * dy1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					y++;
				}
				else
				{
					y--;
				}
				px = px + 2 * (dy1 - dx1);
			}

			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if (safety_check && inv.map_veins_[x][y] == 1) {
				squares_counted++;
			}
		}
	}
	else
	{
		if (dy >= 0)
		{
			x = initial.x;
			y = initial.y;
			ye = final.y;
		}
		else
		{
			x = final.x;
			y = final.y;
			ye = initial.y;
		}
		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
		if (safety_check && inv.map_veins_[x][y] == 1) {
			squares_counted++;
		}

		for (int i = 0; y<ye; i++)
		{
			y = y + 1;
			if (py <= 0)
			{
				py = py + 2 * dx1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					x++;
				}
				else
				{
					x--;
				}
				py = py + 2 * (dx1 - dy1);
			}
			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if ( safety_check && inv.map_veins_[x][y] == 1) {
				squares_counted++;
			}

		}
	}

	return squares_counted;
}


double CUNYAIModule::getProperSpeed( const Unit u ) {
    UnitType u_type = u->getType();
    Player owner = u->getPlayer();

    double base_speed = u_type.topSpeed();
    if (u_type == UnitTypes::Zerg_Zergling && owner->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Zerg_Overlord && owner->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Muscular_Augments) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Zerg_Ultralisk && owner->getUpgradeLevel(UpgradeTypes::Anabolic_Synthesis) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Protoss_Scout && owner->getUpgradeLevel(UpgradeTypes::Gravitic_Thrusters) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Protoss_Zealot && owner->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) > 0) {
        base_speed *= 1.5;
    }

    return base_speed;
}

double CUNYAIModule::getProperSpeed(const UnitType &type, const Player owner) {
    double base_speed = type.topSpeed();

    if (type == UnitTypes::Zerg_Zergling && owner->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Zerg_Overlord && owner->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Muscular_Augments) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Zerg_Ultralisk && owner->getUpgradeLevel(UpgradeTypes::Anabolic_Synthesis) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Protoss_Scout && owner->getUpgradeLevel(UpgradeTypes::Gravitic_Thrusters) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Protoss_Zealot && owner->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) > 0) {
        base_speed *= 1.5;
    }
    return base_speed;
}

int CUNYAIModule::getProperRange(const Unit u) {

    UnitType u_type = u->getType();
    Player owner = u->getPlayer();

    int base_range = max(u_type.groundWeapon().maxRange(), u_type.airWeapon().maxRange());

    if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Grooved_Spines) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Dragoon && owner->getUpgradeLevel(UpgradeTypes::Singularity_Charge) > 0) {
        base_range += 2 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Reaver) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Carrier) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Terran_Marine && owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Terran_Goliath && owner->getUpgradeLevel(UpgradeTypes::Charon_Boosters) > 0) {
        base_range += 3 * 32;
    }
    else if ( u_type == UnitTypes::Terran_Barracks ) {
        base_range = UnitTypes::Terran_Marine.groundWeapon().maxRange() + (owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) * 32;
    }

    return base_range;
}

int CUNYAIModule::getProperRange(const UnitType u_type, const Player owner) {
    int base_range = max(u_type.groundWeapon().maxRange(), u_type.airWeapon().maxRange());
    if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Grooved_Spines) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Dragoon && owner->getUpgradeLevel(UpgradeTypes::Singularity_Charge) > 0) {
        base_range += 2 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Reaver) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Carrier) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Terran_Marine && owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Terran_Goliath && owner->getUpgradeLevel(UpgradeTypes::Charon_Boosters) > 0) {
        base_range += 3 * 32;
    }
    else if ( u_type == UnitTypes::Terran_Barracks ) {
        base_range = UnitTypes::Terran_Marine.groundWeapon().maxRange() + (owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) * 32;
    }

    return base_range;
}

int CUNYAIModule::getChargableDistance(const Unit & u, const Unit_Inventory & ei_loc)
{
    return (u->getType() != UnitTypes::Zerg_Lurker) * (int)CUNYAIModule::getProperSpeed(u) * (int)ei_loc.max_cooldown_ + CUNYAIModule::getProperRange(u) ; //lurkers have a proper speed of 0.
}


//finds nearest choke or best location within 100 minitiles.
Position CUNYAIModule::getNearestChoke( const Position &initial, const Position &final, const Inventory &inv ) {
    WalkPosition e_position = WalkPosition( final );
    WalkPosition wk_postion = WalkPosition( initial );
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );

    int max_observed = inv.map_veins_[wk_postion.x][wk_postion.y];
    Position nearest_choke; 

    for ( auto i = 0; i < 100; ++i ) {
        for ( int x = -1; x <= 1; ++x ) {
            for ( int y = -1; y <= 1; ++y ) {

                int testing_x = wk_postion.x + x;
                int testing_y = wk_postion.y + y;

                if ( !(x == 0, y == 0) &&
                    testing_x < map_dim.x &&
                    testing_y < map_dim.y &&
                    testing_x > 0 &&
                    testing_y > 0 ) { // check for being within reference space.

                    int temp = inv.map_veins_[testing_x][testing_y];

                    if ( temp >= max_observed ) {
                        max_observed = temp;
                        nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
                        wk_postion = WalkPosition( nearest_choke ); //search from there.
                        if ( max_observed > 175 ) {
                            return nearest_choke;
                            break;
                        }
                    }
                }
            }
        }
    }

    //another attempt
    //int x_dist_to_e = e_position.x - wk_postion.x;
    //int y_dist_to_e = e_position.y - wk_postion.y;

    //int dx = x_dist_to_e > 0 ? 1 : -1;
    //int dy = y_dist_to_e > 0 ? 1 : -1;

    //for ( auto i = 0; i < 50; ++i ) {
    //    for ( int x = 0; x <= 1; ++x ) {
    //        for ( int y = 0; y <= 1; ++y ) {

    //            int testing_x = wk_postion.x + x * dx;
    //            int testing_y = wk_postion.y + y * dy;

    //            if ( !(x == 0, y == 0) &&
    //                testing_x < map_dim.x &&
    //                testing_y < map_dim.y &&
    //                testing_x > 0 &&
    //                testing_y > 0 ) { // check for being within reference space.

    //                int temp = inv.map_veins_[testing_x][testing_y];

    //                if ( temp > max_observed ) {
    //                    max_observed = temp;
    //                    nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
    //                    wk_postion = WalkPosition( nearest_choke ); //search from there.
    //                    if ( max_observed > 275 ) {
    //                        return nearest_choke;
    //                        break;
    //                    }
    //                }
    //                else if ( y_dist_to_e == 0 || abs( x_dist_to_e / y_dist_to_e ) > 1 ) {
    //                    dx = x_dist_to_e > 0 ? 1 : -1;
    //                }
    //                else {
    //                    dy = y_dist_to_e > 0 ? 1 : -1;
    //                }
    //            }
    //        }
    //    }
    //}
     //another attempt
    //int x_dist_to_e, y_dist_to_e, dx, dy, x_inc, y_inc;

    //dx = x_dist_to_e > 0 ? 1 : -1;
    //dy = y_dist_to_e > 0 ? 1 : -1;

    //for ( auto i = 0; i < 50; ++i ) {

    //    x_dist_to_e = e_position.x - wk_postion.x;
    //    y_dist_to_e = e_position.y - wk_postion.y;

    //    int testing_x = wk_postion.x + dx;
    //    int testing_y = wk_postion.y + dy;

    //    if ( testing_x < map_dim.x &&

    //        testing_y < map_dim.y &&
    //        testing_x > 0 &&
    //        testing_y > 0 ) { // check for being within reference space.

    //        int temp = inv.map_veins_[testing_x][testing_y];

    //        if ( temp > max_observed ) {
    //            max_observed = temp;
    //            nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
    //            wk_postion = WalkPosition( nearest_choke ); //search from there.
    //            if ( max_observed > 275 ) {
    //                return nearest_choke;
    //                break;
    //            }
    //        }
    //        else if ( abs(y_dist_to_e / x_dist_to_e) < 1 ) {
    //            dx += x_dist_to_e > 0 ? 1 : -1;
    //        }
    //        else {
    //            dy += y_dist_to_e > 0 ? 1 : -1;
    //        }
    //    }
    //}

    return nearest_choke;
}

Position CUNYAIModule::getUnit_Center(Unit unit){
	return Position(unit->getPosition().x + unit->getType().dimensionLeft(), unit->getPosition().y + unit->getType().dimensionUp());
}

// checks if a location is safe and doesn't block minerals.
bool CUNYAIModule::checkSafeBuildLoc(const Position pos, const Inventory &inv, const Unit_Inventory &ei,const Unit_Inventory &ui, Resource_Inventory &ri) {
    Unit_Inventory e_loc = getUnitInventoryInRadius(ei, pos, 750);
    Stored_Unit* e_closest = getClosestThreatOrTargetStored(e_loc, UnitTypes::Zerg_Drone, pos, 750);
    //Stored_Resource* r_closest = getClosestStored(ri,pos, 128); //note this is not from center of unit, it's from upper left.
    Unit_Inventory e_too_close = getUnitInventoryInRadius(ei, pos, 250);
    Unit_Inventory friend_loc = getUnitInventoryInRadius(ui, pos, 750);
    int radial_distance_to_closest_enemy = 0;
    int radial_distance_to_build_position = 0;
    bool enemy_has_not_penetrated = true;
    bool can_still_save = true;
    bool have_to_save = false;

    if ( e_loc.stock_fighting_total_ > 0 && e_closest ) {
        radial_distance_to_closest_enemy = inv.getRadialDistanceOutFromHome(e_closest->pos_);
        radial_distance_to_build_position = inv.getRadialDistanceOutFromHome(pos);
        enemy_has_not_penetrated = radial_distance_to_closest_enemy > radial_distance_to_build_position;
        can_still_save = e_too_close.stock_fighting_total_ > ui.stock_fighting_total_; // can still save it or you don't have a choice.
        have_to_save = inv.min_fields_ <= 12 || inv.getRadialDistanceOutFromHome(pos) < 20000 || inv.hatches_ == 1;
    }

    return enemy_has_not_penetrated && (can_still_save || have_to_save) ;
}

bool CUNYAIModule::checkSafeMineLoc(const Position pos, const Unit_Inventory &ui, const Inventory &inv) {

    bool desperate_for_minerals = inv.min_fields_ < 6;
    bool safe_mine = checkOccupiedArea(ui, pos,250);
    return  safe_mine || desperate_for_minerals;
}

//Checks if enemy air units represent a potential problem. Note: does not check if they HAVE air units.
bool CUNYAIModule::checkWeakAgainstAir(const Unit_Inventory &ui, const Unit_Inventory &ei) {
    //bool u_relatively_weak_against_air = ei.stock_fliers_ / (double)(ui.stock_shoots_up_ + 1) vs ei.stock_ground_units_ / (double)(ui.stock_shoots_down_ + 1); // div by zero concern. The larger one is the BIGGER problem.
    return -ei.stock_fliers_ / (double)pow((ui.stock_shoots_up_ + 1), 2) < -ei.stock_ground_units_ / (double)pow((ui.stock_shoots_down_ + 1), 2); // div by zero concern. Derivative of the above equation, which ratio is shrunk the most?
}

double CUNYAIModule::bindBetween(double x, double lower_bound, double upper_bound) {
    if (lower_bound >= upper_bound) {
        throw std::invalid_argument("lower bound is greater than or equal to upper bound");
    }
    if (x > upper_bound) {
        return upper_bound;
    }
    else if (x < lower_bound) {
        return lower_bound;
    }
    return x;
}
//Zerg_Carapace = 3,
//Zerg_Melee_Attacks = 10,
//Zerg_Missile_Attacks = 11,
//Antennae = 25,
//Pneumatized_Carapace = 26,
//Metabolic_Boost = 27,
//Adrenal_Glands = 28,
//Muscular_Augments = 29,
//Grooved_Spines = 30,
//Chitinous_Plating = 52,
//Anabolic_Synthesis = 53,


