/*
 * ============================================================================
 * Monte Carlo Simulator - Main Execution File
 * ============================================================================
 *
 * HOW TO BUILD AND RUN:
 * ---------------------
 * This project uses CMake with automatic game module selection.
 *
 * METHOD 1: Quick Build Scripts (Recommended)
 * --------------------------------------------
 * For SS03Game:
 *   ./build_ss03.sh
 *   ./build/simulator
 *
 * For DeepDive:
 *   ./build_deepdive.sh
 *   ./build/simulator
 *
 * METHOD 2: VS Code (Default: SS03Game)
 * --------------------------------------
 * 1. Press Cmd+Shift+P
 * 2. Select "Tasks: Run Build Task"
 * 3. Choose "Build Project Simulator (SS03Game)" or "(DeepDive)"
 * 4. Press F5 to run/debug
 *
 * METHOD 3: Manual CMake
 * ----------------------
 * For SS03Game:
 *   cmake -B build -DGAME_MODULE=SS03Game -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15
 *   cmake --build build
 *   ./build/simulator
 *
 * For DeepDive:
 *   cmake -B build -DGAME_MODULE=DeepDive -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15
 *   cmake --build build
 *   ./build/simulator
 *
 * SWITCHING BETWEEN GAMES:
 * ------------------------
 * The game module is selected at COMPILE TIME, not in this file.
 *
 * 1. Choose which game you want to simulate
 * 2. Build with the appropriate method above
 * 3. Update the config file path below (line ~115)
 * 4. Run ./build/simulator
 *
 * You do NOT need to:
 * - Edit include statements (automatic via GameModule.h)
 * - Manually switch between DeepDive.h and SS03Game.h
 * - Clean build when switching (just rebuild)
 *
 * GAME-SPECIFIC CONFIGURATIONS:
 * ------------------------------
 * SS03Game:
 *   - Config file: "SS03_Merged_Flattened_Output.json"
 *   - JSON format: [index, value, trigger_num, levels] for BG
 *                  [index, value, retrigger_num, levels] for FG
 *
 * DeepDive:
 *   - Config file: "SS02_Config_Table01_v1.json" (or similar)
 *   - JSON format: [index, value, flag, levels] for BG
 *                  [index, value, flag, count, levels] for FG
 *   - Also requires: multiplier_pools, item_to_pool_map
 *
 * QUICK TROUBLESHOOTING:
 * ----------------------
 * Q: "array index 4 out of range" error?
 * A: You're running an old binary. Rebuild: ./build_ss03.sh
 *
 * Q: Namespace conflicts?
 * A: Clean rebuild: rm -rf build && ./build_ss03.sh
 *
 * Q: Wrong game module loaded?
 * A: Make sure you built with the correct game module
 *
 * For more details, see BUILD.md and SETUP_SUMMARY.md
 * ============================================================================
 */

#include "MonteCarloSimulator.h"
#include <iostream>
#include <vector>


int main() {
    try {
        // --- Configuration ---
        const int base_bet = 20;
        const long long numSimulations = 1000000000;  // Total rounds (k × m)
        const long long batches = 1000;               // Number of batches (k)
        const long long batch_rounds = numSimulations/batches;  // Rounds per batch (m)

        // ⚠️ MEMORY USAGE WARNING:
        // ACCURATE Mode requires ~77 bytes per simulation (all stored in RAM)
        //   - 1 billion simulations  ≈ 77 GB RAM
        //   - 100 million simulations ≈ 7.7 GB RAM
        //   - 10 million simulations  ≈ 770 MB RAM
        //
        // EFFICIENT Mode requires ~100 MB fixed (regardless of simulation count)
        //
        // Number of batches does NOT affect memory usage.
        // Memory depends ONLY on total simulations (numSimulations = batches × batch_rounds)
        //
        // Recommendation:
        //   - Use EFFICIENT mode for 100M+ simulations (production runs)
        //   - Use ACCURATE mode only if you have sufficient RAM and need exact percentiles

        // ========================================================================
        // 🎮 GAME CONFIGURATION FILE
        // ========================================================================
        // IMPORTANT: The config file must match the game module you built with!
        //
    // For SS03Game (current default):
    //      gameType => "SS03" uses SS03 flattened config
    // For DeepDive:
    //      gameType => "SS02" uses DeepDive configs
#if defined(USE_SS03GAME)
    static constexpr const char* kGameType = "SS03";
    static constexpr const char* kConfigPath = "SS03_Config_Table01_v1.json"; // Default SS03 configuration
#elif defined(USE_DEEPDIVE)
    static constexpr const char* kGameType = "SS02";
    static constexpr const char* kConfigPath = "SS02_Config_Table01_v1.json";       // Default DeepDive configuration
#else
#error "Unknown game module selected at build time"
#endif
    const std::string gameType{kGameType};
    const std::string configFile{kConfigPath};
        // ========================================================================
        // --- Toggle for Parallel Processing ---
        const bool useParallel = true;
        // --- Optional factors to scale BG and FG item values during import ---
        const double bg_value_factor = 1.0; // e.g., 0.9 to reduce all BG values by 10%
        const double fg_value_factor = 1.0; // Keep FG values the same
        // --- Simulation Mode and Second Chance Probability ---
        const Game::SimulationMode sim_mode = Game::SimulationMode::FULL_GAME; // Options: FULL_GAME, FG_ONLY, BG_ONLY
        const double second_chance_prob = 0.00;//47; //46;  e.g., 0.5% chance

        // --- Initialization ---
        std::cout << "[Init] Game Type: " << gameType
                  << " | Config File: " << configFile << std::endl;
        Game::initializeFromJSON(configFile,bg_value_factor,fg_value_factor);
        //MonteCarloSimulator simulator1;
        MonteCarloSimulator simulator2;

        // --- CHOOSE YOUR HISTOGRAM STRATEGY (for EFFICIENT mode) ---
        // Only one of these sections should be active.

        // Option 1 (Recommended): Progressive Bins
        //simulator.setProgressiveHistogramBins();

        // Option 2: Custom Bins (Uncomment to use)
        // Define bins as multiples of base_bet for easier configuration
        std::vector<double> bin_multipliers = {1, 5, 10, 20, 35, 50, 100};
        std::vector<double> my_bins;
        for (double mult : bin_multipliers) {
            my_bins.push_back(mult * base_bet);
        }
        //simulator1.setCustomHistogramBins(my_bins);
        simulator2.setCustomHistogramBins(my_bins);
        

        // Option 3: Fixed-Width Bins (Uncomment to use)
        /*
        simulator.setFixedWidthHistogramBins(10000.0, 50); // Bins up to 10k
        */
        
        // --- Execution ---
        //simulator1.run(numSimulations, sim_mode, MemoryMode::ACCURATE, useParallel, second_chance_prob);
        simulator2.run(batches, batch_rounds, sim_mode, MemoryMode::EFFICIENT, useParallel, second_chance_prob);

        //std::cout << "\n========================================" << std::endl;
        //std::cout << "SIMULATOR 1: FALLBACK METHOD (No CI)" << std::endl;
        //std::cout << "========================================" << std::endl;
        //simulator1.printResults(base_bet);

        std::cout << "\n\n========================================" << std::endl;
        std::cout << "SIMULATOR 2: BATCH METHOD (With CI)" << std::endl;
        std::cout << "========================================" << std::endl;
        simulator2.printResults(base_bet);
        

    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown error occurred." << std::endl;
        return 1;
    }

    return 0;
}
