#pragma once

#include "MeatAIModule.h"

//Movement and Combat Functions
class Boids {

public:
    // Basic retreat logic
    void Retreat_Logic( const Unit &unit, const Stored_Unit &e_unit, Unit_Inventory &ei, const Unit_Inventory &ui, Inventory &inventory, const Color &color );
    // Tells the unit to fight. If it can attack both air and ground.
    void Tactical_Logic( const Unit & unit, const Unit_Inventory & ei, const Unit_Inventory &ui, const Color & color );
    //Forces a unit to flock in a boids manner. Initial versions merely stuttered in a brownian manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.
    void Boids_Movement( const Unit &unit, const double &n, const Unit_Inventory &ui, Unit_Inventory &ei, Inventory &inventory, const bool &army_starved );

    //Forces the closest Overlord or Zergling to lock itself onto a mineral patch so I can keep vision of it. Helps expos.
    //void Vision_Locking( const Unit &unit );

    Position Output;

    void setAlignment( const Unit &unit, const Unit_Inventory &ui );
    void setCentralize( const Position &pos, const Inventory &inventory );
    void setStutter( const Unit &unit, const double &n );
    void setCohesion( const Unit &unit, const Position &pos, const Unit_Inventory &ui );
    // towards enemy or enemy base.
    void setAttraction( const Unit &unit, const Position &pos, Unit_Inventory &ei, Inventory &inv, const bool &army_starved );
    void scoutEnemyBase(const Unit & unit, const Position & pos, Unit_Inventory & ei, Inventory & inv);
    // from enemy or towards home.
    void setAttractionHome( const Unit & unit, const Position & pos, Unit_Inventory & ei, Inventory & inv);
    void setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui );
    void setSeperationScout(const Unit & unit, const Position & pos, const Unit_Inventory & ui);
    //void setUnwalkability( const Unit &unit, const Position &pos, const Inventory &inventory );
    void setObjectAvoid( const Unit &unit, const Position &pos, const Inventory &inventory );

    bool Boids::fix_lurker_burrow(const Unit &unit, const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target);

private:
    double x_stutter_ = 0;
    double y_stutter_ = 0;
    double attune_dx_ = 0;
    double attune_dy_ = 0;
    double cohesion_dx_ = 0;
    double cohesion_dy_ = 0;
    double centralization_dx_ = 0;
    double centralization_dy_ = 0;
    double seperation_dx_ = 0;
    double seperation_dy_ = 0;
    double attract_dx_ = 0;
    double attract_dy_ = 0;
    double walkability_dx_ = 0;
    double walkability_dy_ = 0;

    int rng_direction_ ; // send unit in a random tilt direction if blocked

};
