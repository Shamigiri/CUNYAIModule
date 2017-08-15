#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\MeatAIModule.h"
# include "Source\GeneticHistoryManager.h"
# include <fstream>
# include <BWAPI.h>
# include <cstdlib>
# include <cstring>
# include <ctime>
# include <string>
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

// Returns average of historical wins against that race for key heuristic values. For each specific value:[0...5] : { delta_out, gamma_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };
GeneticHistory::GeneticHistory( string file ) {

    //srand( Broodwar->getRandomSeed() ); // don't want the BW seed if the seed is locked. 

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( 0, 1 );

    // default values for output.
    double delta_out = dis( gen );
    double gamma_out = dis( gen );

    // the values below will be normalized to 1.
    double a_army_out = dis( gen );
    double a_vis_out  = dis( gen );
    double a_econ_out = dis( gen );
    double a_tech_out = dis( gen );

    int win_count = 0;
    int zerg_count = 0;
    int terran_count = 0;
    int protoss_count = 0;
    int random_count = 0;
    int relevant_game_count = 0;

    string entry; // entered string from stream
    vector<double> delta_in;
    vector<double> gamma_in;
    vector<double> a_army_in;
    vector<double> a_vis_in;
    vector<double> a_econ_in;
    vector<double> a_tech_in;
    vector<string> race_in;

    vector<int> win_in;
    vector<int> sdelay_in;
    vector<int> mdelay_in;
    vector<int> ldelay_in;
    vector<int> seed_in;

    vector<double> delta_win;
    vector<double> gamma_win;
    vector<double> a_army_win;
    vector<double> a_vis_win;
    vector<double> a_econ_win;
    vector<double> a_tech_win;

    double loss_rate = 1;

    ifstream input; // brings in info;
    input.open( file, ios::in );
    // for each row, m8
    
    string line;
    int csv_length = 0;
    while( getline( input, line ) ) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open( file, ios::in );

        for ( int j = 0; j < csv_length; ++j ) { // further brute force inelegance.

            getline( input, entry, ',' );
            delta_in.push_back( stod( entry ) );
            getline( input, entry, ',' );
            gamma_in.push_back( stod( entry ) );
            getline( input, entry, ',' );
            a_army_in.push_back( stod( entry ) );
            getline( input, entry, ',' );
            a_vis_in.push_back( stod( entry ) );
            getline( input, entry, ',' );
            a_econ_in.push_back( stod( entry ) ); 
            getline( input, entry, ',' );
            a_tech_in.push_back( stod( entry ) );

            getline( input, entry, ',' );
            race_in.push_back( entry );

            getline( input, entry, ',' );
            win_in.push_back( stoi( entry ) );
            getline( input, entry, ',' );
            sdelay_in.push_back( stoi( entry ) );
            getline( input, entry, ',' );
            mdelay_in.push_back( stoi( entry ) );
            getline( input, entry, ',' );
            ldelay_in.push_back( stoi( entry ) );

            getline( input, entry ); //diff. End of line char, not ','
            seed_in.push_back( stoi( entry ) );
        } // closure for each row

        for ( int j = 0; j < csv_length; ++j ) {
            //count wins.
            if ( (win_in[j] == 1 /*|| sdelay_in[j] >= 321*/) && race_in[j] == "Zerg" && Broodwar->enemy()->getRace() == Races::Zerg ) {
                delta_win.push_back( delta_in[j] );
                gamma_win.push_back( gamma_in[j] );
                a_army_win.push_back( a_army_in[j] );
                a_vis_win.push_back( a_vis_in[j] );
                a_econ_win.push_back( a_econ_in[j] );
                a_tech_win.push_back( a_tech_in[j] );
                win_count++;
            }
            else if ( (win_in[j] == 1/* || sdelay_in[j] >= 321*/) && race_in[j] == "Protoss" && Broodwar->enemy()->getRace() == Races::Protoss ) {
                delta_win.push_back( delta_in[j] );
                gamma_win.push_back( gamma_in[j] );
                a_army_win.push_back( a_army_in[j] );
                a_vis_win.push_back( a_vis_in[j] );
                a_econ_win.push_back( a_econ_in[j] );
                a_tech_win.push_back( a_tech_in[j] );
                win_count++;
            }
            else if ( (win_in[j] == 1 /*|| sdelay_in[j] >= 321*/) && race_in[j] == "Terran" && Broodwar->enemy()->getRace() == Races::Terran ) {
                delta_win.push_back( delta_in[j] );
                gamma_win.push_back( gamma_in[j] );
                a_army_win.push_back( a_army_in[j] );
                a_vis_win.push_back( a_vis_in[j] );
                a_econ_win.push_back( a_econ_in[j] );
                a_tech_win.push_back( a_tech_in[j] );
                win_count++;
            }
            else if ( (win_in[j] == 1 /*|| sdelay_in[j] >= 321*/) && race_in[j] == "Random" && Broodwar->enemy()->getRace() == Races::Random ) {
                delta_win.push_back( delta_in[j] );
                gamma_win.push_back( gamma_in[j] );
                a_army_win.push_back( a_army_in[j] );
                a_vis_win.push_back( a_vis_in[j] );
                a_econ_win.push_back( a_econ_in[j] );
                a_tech_win.push_back( a_tech_in[j] );
                win_count++;
            }

            //count games.
            if ( race_in[j] == "Zerg" ) {
                zerg_count++;
            }
            else if ( race_in[j] == "Protoss" ) {
                protoss_count++;
            }
            else if ( race_in[j] == "Terran" ) {
                terran_count++;
            }
            else if ( race_in[j] == "Random" ) {
                random_count++;
            }
        }

        if ( win_count > 0 ) { // redefine final output.

            int parent_1 = dis( gen ) * win_count; // safe even if there is only 1 win., index starts at 0.
            int parent_2 = dis( gen ) * win_count;

            double l = dis( gen ); //linear_crossover, interior of parents. Big mutation at the end, though.
            delta_out  = l * delta_win[parent_1]  + (1 - l) * delta_win[parent_2];
            gamma_out  = l * gamma_win[parent_1]  + (1 - l) * gamma_win[parent_2];
            a_army_out = l * a_army_win[parent_1] + (1 - l) * a_army_win[parent_2];
            a_vis_out  = l * a_vis_win[parent_1]  + (1 - l) * a_vis_win[parent_2];
            a_econ_out = l * a_econ_win[parent_1] + (1 - l) * a_econ_win[parent_2];
            a_tech_out = l * a_tech_win[parent_1] + (1 - l) * a_tech_win[parent_2];

            //Gene swapping between parents. Not as popular for continuous optimization problems.
            //int chrom_0 = (rand() % 100 + 1) / 2;
            //int chrom_1 = (rand() % 100 + 1) / 2;
            //int chrom_2 = (rand() % 100 + 1) / 2;
            //int chrom_3 = (rand() % 100 + 1) / 2;
            //int chrom_4 = (rand() % 100 + 1) / 2;
            //int chrom_5 = (rand() % 100 + 1) / 2;

            //double delta_out_temp = chrom_0 > 50 ? delta_win[parent_1] : delta_win[parent_2];
            //double gamma_out_temp = chrom_1 > 50 ? gamma_win[parent_1] : gamma_win[parent_2];
            //double a_army_out_temp = chrom_2 > 50 ? a_army_win[parent_1] : a_army_win[parent_2];
            //double a_vis_out_temp = chrom_3 > 50 ? a_vis_win[parent_1] : a_vis_win[parent_2];
            //double a_econ_out_temp = chrom_4 > 50 ? a_econ_win[parent_1] : a_econ_win[parent_2];
            //double a_tech_out_temp = chrom_5 > 50 ? a_tech_win[parent_1] : a_tech_win[parent_2];
        }

        for( int i = 0; i<1000; i++){  // no corner solutions, please. Happens with incredibly small values 2*10^-234 ish.

            //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.
            if ( win_count > 0 ) { // if we have a win to work with

                if ( Broodwar->enemy()->getRace() == Races::Zerg )
                {
                    relevant_game_count = zerg_count;
                }
                else if ( Broodwar->enemy()->getRace() == Races::Terran ) {
                    relevant_game_count = terran_count;
                }
                else if ( Broodwar->enemy()->getRace() == Races::Protoss ) {
                    relevant_game_count = protoss_count;
                }
                else if ( Broodwar->enemy()->getRace() == Races::Random ) {
                    relevant_game_count = random_count;
                }

                double loss_rate_temp = 1 - win_count / (double)relevant_game_count;
                if ( loss_rate_temp != 0 ) {
                    loss_rate_ = loss_rate_temp; // Don't set all your parameters to zero on game two if you win game one.
                }
            }

            //From genetic history, random parent for each gene. Mutate the genome
            int mutation_0 = dis( gen ) * 3; // rand int between 0-2
            int mutation_1 = dis( gen ) * 3; // rand int between 0-2

            double mutation = pow(loss_rate,2) * (dis( gen ) + 0.5); // will generate rand double between 0.5 and 1.5.  Times it by loss rate squared.
            delta_out_mutate_  = mutation_0 == 0 ? delta_out  * mutation : delta_out;
            gamma_out_mutate_  = mutation_0 == 1 ? gamma_out  * mutation : gamma_out;
            a_vis_out_mutate_  = mutation_0 == 2 ? a_vis_out  * mutation : a_vis_out;// currently does nothing, vision is an artifact atm.

            a_army_out_mutate_ = mutation_1 == 0 ? a_army_out * mutation : a_army_out;
            a_econ_out_mutate_ = mutation_1 == 1 ? a_econ_out * mutation : a_econ_out;
            a_tech_out_mutate_ = mutation_1 == 2 ? a_tech_out * mutation : a_tech_out;

            // Normalize the CD part of the gene.
            double a_tot = a_army_out_mutate_ + a_econ_out_mutate_ + a_tech_out_mutate_;
            a_army_out_mutate_ = a_army_out_mutate_ / a_tot;
            a_econ_out_mutate_ = a_econ_out_mutate_ / a_tot;
            a_tech_out_mutate_ = a_tech_out_mutate_ / a_tot;

            if ( a_army_out_mutate_ > 0.01 && a_econ_out_mutate_ > 0.01 && a_tech_out_mutate_ > 0.01 && delta_out_mutate_ < 1 && delta_out_mutate_ > 0.01 && gamma_out_mutate_ < 1 && gamma_out_mutate_ > 0.01) {
                break; // if we have an interior solution, let's use it, if not, we try again.
            }
        }

}