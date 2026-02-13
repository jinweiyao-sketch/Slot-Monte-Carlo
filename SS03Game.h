#ifndef SS03Game_H
#define SS03Game_H

#include <vector>
#include <string>
#include <random>
#include <unordered_map>
#include <atomic> // <-- ADDED: For thread-safe initialization flag

namespace Game {

    // Represents an item in the "BG" vector: <index, values, trigger_num>
    struct BG_Item {
        int index;
        int value;
        int trigger_num;
        int levels;
    };

    // Represents an item in the "FG" vector: <index, values, trigger_num>
    struct FG_Item {
        int index;
        int value;
        int retrigger_num;
        int levels;
    };



    // --- Internal Game Data Storage ---
    struct GameData {
        std::vector<BG_Item> bg_items;
        std::vector<FG_Item> fg_items;
    };


    // --- =Result Struct ---
    // This struct will be returned by each game round to bundle the
    // score with the new run length statistic.
    struct GameResult {
        double bg_score;
        double fg_score;
        long long fg_run_length;         // Total number of FG picks in this session
        bool fg_was_triggered;
        long long fg_nonzero_picks;      // Count of FG picks with nonzero value in this session
        long long max_bg_multiplier = 5; // In this case, depends on levels, possible values are 1, 2, 3, 5
        long long max_fg_multiplier = 10;// In this case, depends on levels, possible values are 2, 4, 6, 10
        int bg_levels;                   // The level count of selected bg_item 
        std::vector<int> fg_levels;      // The level counts of all fg_items selected. 
    };


    enum class SimulationMode {
        FULL_GAME,
        FG_ONLY,
        BG_ONLY
    };

    // --- Game Module Interface ---
    void initializeWithSampleData();
    /**
     * @brief Initializes the game state by loading data from a JSON file.
     * @param filename The path to the JSON configuration file.
     * @param bg_value_factor A factor to multiply every BG item's value by. Defaults to 1.0.
     * @param fg_value_factor A factor to multiply every FG item's value by. Defaults to 1.0.
     */
    void initializeFromJSON(
        const std::string& filename, 
        double bg_value_factor = 1.0, 
        double fg_value_factor = 1.0
    );

    GameResult simulateGameRound(std::mt19937& rng, SimulationMode mod, double second_chance_prob);

    // HIGHLIGHT: Added a public "getter" function to safely access the game data.
    const GameData& getGameData();


} // namespace Game

#endif // SS03Game_H
