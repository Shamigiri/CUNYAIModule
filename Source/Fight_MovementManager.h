#pragma once

#include "CUNYAIModule.h"
#include "Unit_Inventory.h"

//Movement and Combat Functions
class Mobility {

public:
    // Basic retreat logic
    void Retreat_Logic(const Unit &unit, const Stored_Unit &e_unit, const Unit_Inventory &u_squad, Unit_Inventory &e_squad, Unit_Inventory &ei, const Unit_Inventory &ui, const int &passed_distance, const Map_Inventory &inv, const Color &color, const bool &force);
    // Tells the unit to fight. If it can attack both air and ground.
    void Tactical_Logic(const Unit & unit, const Stored_Unit &e_unit, Unit_Inventory & ei, const Unit_Inventory &ui, const int &passed_dist, const Map_Inventory &inv, const Color & color);
    //Forces a unit to flock in a (previously) Mobility manner. Will attack if it sees something.
    void Pathing_Movement( const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, const int &passed_distance, const Position &e_pos, const Map_Inventory &inv );
    //Forces a unit to surround the concerning ei. Does not advance.
    //void Surrounding_Movement(const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, const Map_Inventory &inv);


    Position Output;

    // Causes a unit to match headings with neighboring units.
    Position setAlignment( const Unit &unit, const Unit_Inventory &ui );
    // Causes UNIT to run directly from enemy.
    Position setDirectRetreat(const Position & pos, const Position &e_pos, const UnitType & type);
    // Causes a unit to move towards central map veins.
    Position setCentralize( const Position &pos, const Map_Inventory &inv );
    // causes a unit to move about in a random (brownian) fashion.
    Position setStutter( const Unit &unit, const double &n );
    // Causes a unit to be pulled towards others of their kind.
    Position setCohesion( const Unit &unit, const Position &pos, const Unit_Inventory &ui );
    // causes a unit to be pulled towards (map) center.
    Position setAttraction(const Unit & unit, const Position & pos, const Map_Inventory & inv, const vector<vector<int>>& map, const Position &map_center);
    // causes a unit to be pushed away from (map) center. Dangerous for ground units, could lead to them running down dead ends.
    Position setRepulsion(const Unit & unit, const Position & pos, const Map_Inventory & inv, const vector<vector<int>>& map, const Position & map_center);
    // causes a unit to move directly towards the enemy base.
    Position scoutEnemyBase(const Unit & unit, const Position & pos, Map_Inventory & inv);
    // causes a unit to seperate itself from others.
    Position setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui );
    // causes a unit to seperate itself from others at a distance of its own vision.
    Position setSeperationScout(const Unit & unit, const Position & pos, const Unit_Inventory & ui);
    //void setUnwalkability( const Unit &unit, const Position &pos, const Map_Inventory &inv);
    // Causes a unit to avoid units in its distant future, near future, and immediate position.
    Position setObjectAvoid(const Unit &unit, const Position &current_pos, const Position &future_pos, const Map_Inventory &inv, const vector<vector<int>> &map);
    bool adjust_lurker_burrow(const Unit &unit, const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target);

    // gives a vector that has the direction towards center on (map). Must return a PAIR since it returns a unit vector.
    Position getVectorTowardsMap(const Position & pos, const Map_Inventory & inv, const vector<vector<int>>& map) const;


private:
    int distance_metric = 0;
    Position stutter_vector_ = Positions::Origin;
    Position attune_vector_ = Positions::Origin;
    Position cohesion_vector_ = Positions::Origin;
    Position centralization_vector_ = Positions::Origin;
    Position seperation_vector_ = Positions::Origin;
    Position attract_vector_ = Positions::Origin;
    Position retreat_vector_ = Positions::Origin;
    Position walkability_vector_ = Positions::Origin;

    int rng_direction_ ; // send unit in a random tilt direction if blocked

};
