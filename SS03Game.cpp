#include "SS03Game.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <fstream>
#include <numeric>
#include <iomanip>
#include <map>
#include "json.hpp" // Assumes nlohmann/json library is available

// Use the nlohmann namespace for convenience
using json = nlohmann::json;

namespace Game {

    // --- Internal Game Data Storage (file-local) ---
    static GameData gameData;
    static std::atomic<bool> isInitialized = false;

    // Safety limit to prevent a single game round from using too much memory.
    const size_t MAX_QUEUE_SIZE = 2000;

    // Helper to clear data before loading
    static void clearGameData() {
        gameData.bg_items.clear();
        gameData.fg_items.clear();
        isInitialized = false;
    }

    /**
     * Initializes the game with hardcoded sample data for testing purposes.
     */
    void initializeWithSampleData() {
        clearGameData();
        std::cout << "Initializing SS03Game with hardcoded sample data..." << std::endl;

        gameData.bg_items = {
            {1, 10, 0, 2},
            {2, 20, 0, 3},
            {3, 5, 0, 1},
            {4, 0, 10, 1},
            {5, 0, 12, 1}
        };

        gameData.fg_items = {
            {101, 100, 0, 2},
            {102, 250, 2, 3},
            {103, 50, 0, 1}
        };

        isInitialized = true;
        std::cout << "Sample data initialization complete." << std::endl;
    }

    /**
     * Initializes game data from a JSON file, supporting both object and compact array formats.
     */
    void initializeFromJSON(const std::string& filename, double bg_value_factor, double fg_value_factor) {
        clearGameData();
        std::cout << "Initializing SS03Game data from '" << filename << "'..." << std::endl;
        std::cout << "  BG Value Factor: " << bg_value_factor << std::endl;
        std::cout << "  FG Value Factor: " << fg_value_factor << std::endl;

        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open JSON file: " + filename);
        }

        try {
            json data = json::parse(file);

            // --- Dual Format Parsing for bg_items ---
            const auto& bg_items_json = data.at("bg_items");
            if (!bg_items_json.empty()) {
                if (bg_items_json[0].is_object()) {
                    for (const auto& item : bg_items_json) {
                        gameData.bg_items.push_back({
                            item.at("index").get<int>(),
                            static_cast<int>(item.at("value").get<double>() * bg_value_factor),
                            item.at("trigger_num").get<int>(),
                            item.at("levels").get<int>()
                        });
                    }
                } else if (bg_items_json[0].is_array()) {
                    for (const auto& item_arr : bg_items_json) {
                        gameData.bg_items.push_back({
                            item_arr.at(0).get<int>(),
                            static_cast<int>(item_arr.at(1).get<double>() * bg_value_factor),
                            item_arr.at(2).get<int>(),
                            item_arr.at(3).get<int>()
                        });
                    }
                }
            }

            // --- Dual Format Parsing for fg_items ---
            const auto& fg_items_json = data.at("fg_items");
            if (!fg_items_json.empty()) {
                if (fg_items_json[0].is_object()) {
                    for (const auto& item : fg_items_json) {
                        gameData.fg_items.push_back({
                            item.at("index").get<int>(),
                            static_cast<int>(item.at("value").get<double>() * fg_value_factor),
                            item.at("retrigger_num").get<int>(),
                            item.at("levels").get<int>()
                        });
                    }
                } else if (fg_items_json[0].is_array()) {
                    for (const auto& item_arr : fg_items_json) {
                        gameData.fg_items.push_back({
                            item_arr.at(0).get<int>(),
                            static_cast<int>(item_arr.at(1).get<double>() * fg_value_factor),
                            item_arr.at(2).get<int>(),
                            item_arr.at(3).get<int>()
                        });
                    }
                }
            }
        } catch (json::exception& e) {
            throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
        }

        // --- Descriptive Statistics for Input Data ---
        std::cout << "\n------ Input Data Summary ------" << std::endl;

        // BG Items Stats
        std::cout << "BG Items: " << gameData.bg_items.size() << " entries." << std::endl;

        // Calculate average trigger_num and distribution
        if (!gameData.bg_items.empty()) {
            long long total_trigger_num = 0;
            long long nonzero_trigger_count = 0;
            long long nonzero_trigger_sum = 0;
            std::map<int, int> trigger_distribution;

            for (const auto& item : gameData.bg_items) {
                total_trigger_num += item.trigger_num;
                if (item.trigger_num > 0) {
                    nonzero_trigger_count++;
                    nonzero_trigger_sum += item.trigger_num;
                }
                trigger_distribution[item.trigger_num]++;   //dict for trigger_num distribution
            }

            double avg_trigger_num_total = static_cast<double>(total_trigger_num) / gameData.bg_items.size();
            double avg_trigger_num_nonzero = nonzero_trigger_count == 0 ? 0.0 : static_cast<double>(nonzero_trigger_sum) / nonzero_trigger_count;

            std::cout << "  - Avg Trigger Num: " << std::fixed << std::setprecision(4) << avg_trigger_num_total
                      << " (Excl. 0's: " << avg_trigger_num_nonzero << ")" << std::endl;
            std::cout << "  - Items with Trigger > 0: " << nonzero_trigger_count
                      << " (" << std::fixed << std::setprecision(2)
                      << (100.0 * nonzero_trigger_count / gameData.bg_items.size()) << "%)" << std::endl;

            std::cout << "  - Trigger Distribution:" << std::endl;
            for (const auto& [trigger_val, count] : trigger_distribution) {
                double percentage = 100.0 * count / gameData.bg_items.size();
                std::cout << "      " << trigger_val << ": " << count
                          << " (" << std::fixed << std::setprecision(2) << percentage << "%)" << std::endl;
            }
        }

        // BG Nonzero Values
        long long bg_nonzero_values = 0;
        for (const auto& item : gameData.bg_items) {
            if (item.value != 0) bg_nonzero_values++;
        }
        double bg_nonzero_prob = gameData.bg_items.empty() ? 0.0 : 100.0 * static_cast<double>(bg_nonzero_values) / gameData.bg_items.size();
        std::cout << "  - Nonzero Values: " << bg_nonzero_values
                  << " (" << std::fixed << std::setprecision(2) << bg_nonzero_prob << "%)" << std::endl;

        // BG Levels Stats with data integrity check
        for (const auto& item : gameData.bg_items) {
            if (item.value == 0 && item.levels != 1) {
                std::cout << "  [Warning] BG Item index " << item.index
                          << " has value=0 but levels=" << item.levels << " (expected 1)" << std::endl;
            }
        }

        long long bg_total_levels = 0;
        long long bg_nonzero_value_count = 0;
        long long bg_nonzero_value_levels_sum = 0;
        int bg_max_level = 0;
        for (const auto& item : gameData.bg_items) {
            bg_total_levels += item.levels;
            if (item.value != 0 && item.levels != 1) {
                bg_nonzero_value_count++;
                bg_nonzero_value_levels_sum += item.levels;
            }
            if (item.levels > bg_max_level) bg_max_level = item.levels;
        }
        double bg_avg_level_total = gameData.bg_items.empty() ? 0.0 : static_cast<double>(bg_total_levels) / gameData.bg_items.size();
        double bg_avg_level_nonzero_value = bg_nonzero_value_count == 0 ? 0.0 : static_cast<double>(bg_nonzero_value_levels_sum) / bg_nonzero_value_count;
        std::cout << "  - Levels: Max = " << bg_max_level
                  << ", Avg (Total) = " << std::fixed << std::setprecision(4) << bg_avg_level_total
                  << ", Avg (Nonzero Value) = " << bg_avg_level_nonzero_value << std::endl;

        // FG Items Stats
        std::cout << "FG Items: " << gameData.fg_items.size() << " entries." << std::endl;

        // Calculate average retrigger_num and distribution
        if (!gameData.fg_items.empty()) {
            long long total_retrigger_num = 0;
            long long nonzero_retrigger_count = 0;
            long long nonzero_retrigger_sum = 0;
            std::map<int, int> retrigger_distribution;

            for (const auto& item : gameData.fg_items) {
                total_retrigger_num += item.retrigger_num;
                if (item.retrigger_num > 0) {
                    nonzero_retrigger_count++;
                    nonzero_retrigger_sum += item.retrigger_num;
                }
                retrigger_distribution[item.retrigger_num]++;
            }

            double avg_retrigger_num_total = static_cast<double>(total_retrigger_num) / gameData.fg_items.size();
            double avg_retrigger_num_nonzero = nonzero_retrigger_count == 0 ? 0.0 : static_cast<double>(nonzero_retrigger_sum) / nonzero_retrigger_count;

            std::cout << "  - Avg Retrigger Num: " << std::fixed << std::setprecision(4) << avg_retrigger_num_total
                      << " (Excl. 0's: " << avg_retrigger_num_nonzero << ")" << std::endl;
            std::cout << "  - Items with Retrigger > 0: " << nonzero_retrigger_count
                      << " (" << std::fixed << std::setprecision(2)
                      << (100.0 * nonzero_retrigger_count / gameData.fg_items.size()) << "%)" << std::endl;

            std::cout << "  - Retrigger Distribution:" << std::endl;
            for (const auto& [retrigger_val, count] : retrigger_distribution) {
                double percentage = 100.0 * count / gameData.fg_items.size();
                std::cout << "      " << retrigger_val << ": " << count
                          << " (" << std::fixed << std::setprecision(2) << percentage << "%)" << std::endl;
            }
        }

        // FG Nonzero Values
        long long fg_nonzero_values = 0;
        for (const auto& item : gameData.fg_items) {
            if (item.value != 0) fg_nonzero_values++;
        }
        double fg_nonzero_prob = gameData.fg_items.empty() ? 0.0 : 100.0 * static_cast<double>(fg_nonzero_values) / gameData.fg_items.size();
        std::cout << "  - Nonzero Values: " << fg_nonzero_values
                  << " (" << std::fixed << std::setprecision(2) << fg_nonzero_prob << "%)" << std::endl;

        // FG Levels Stats with data integrity check
        for (const auto& item : gameData.fg_items) {
            if (item.value == 0 && item.levels != 1) {
                std::cout << "  [Warning] FG Item index " << item.index
                          << " has value=0 but levels=" << item.levels << " (expected 1)" << std::endl;
            }
        }

        long long fg_total_levels = 0;
        long long fg_nonzero_value_count = 0;
        long long fg_nonzero_value_levels_sum = 0;
        int fg_max_level = 0;
        for (const auto& item : gameData.fg_items) {
            fg_total_levels += item.levels;
            if (item.value != 0 && item.levels != 1) {
                fg_nonzero_value_count++;
                fg_nonzero_value_levels_sum += item.levels;
            }
            if (item.levels > fg_max_level) fg_max_level = item.levels;
        }
        double fg_avg_level_total = gameData.fg_items.empty() ? 0.0 : static_cast<double>(fg_total_levels) / gameData.fg_items.size();
        double fg_avg_level_nonzero_value = fg_nonzero_value_count == 0 ? 0.0 : static_cast<double>(fg_nonzero_value_levels_sum) / fg_nonzero_value_count;
        std::cout << "  - Levels: Max = " << fg_max_level
                  << ", Avg (Total) = " << std::fixed << std::setprecision(4) << fg_avg_level_total
                  << ", Avg (Nonzero Value) = " << fg_avg_level_nonzero_value << std::endl;

        std::cout << "--------------------------------" << std::endl;

        isInitialized = true;
        std::cout << "JSON data initialization complete." << std::endl;
    }

    /**
     * Simulates a single round of the new game, returning a detailed result.
     * NOTE: The calling MonteCarloSimulator will need to be updated to handle
     * the 'GameResult' struct instead of a simple 'double'. For now, it might
     * just sum result.bg_score + result.fg_score.
     */
    GameResult simulateGameRound(std::mt19937& rng, SimulationMode mode, double second_chance_prob) {
        if (!isInitialized) {
            throw std::runtime_error("FATAL: Game logic called before data was initialized.");
        }

        GameResult result = {0.0, 0.0, 0, false, 0, 1, 1, 0, {}};
        int initial_triggers = 0;

        // --- Step 1: Handle the simulation mode ---

        // BG_ONLY mode is simple: just pick a BG item and return its score.
        if (mode == SimulationMode::BG_ONLY) {
            if (gameData.bg_items.empty()) return result;
            std::uniform_int_distribution<size_t> bg_dist(0, gameData.bg_items.size() - 1);
            const BG_Item& chosen_bg = gameData.bg_items[bg_dist(rng)];
            result.bg_score = chosen_bg.value;
            result.bg_levels = chosen_bg.levels;
            // Calculate max_bg_multiplier based on levels: {1→1, 2→2, 3→3, ≥4→5}
            if (chosen_bg.levels <= 0) {
                result.max_bg_multiplier = 1; // Safety: unexpected case, default to 1
            } else if (chosen_bg.levels == 1) {
                result.max_bg_multiplier = 1;
            } else if (chosen_bg.levels == 2) {
                result.max_bg_multiplier = 2;
            } else if (chosen_bg.levels == 3) {
                result.max_bg_multiplier = 3;
            } else { // levels >= 4
                result.max_bg_multiplier = 5;
            }
            return result;
        }

        // FG_ONLY mode starts the FG sequence directly with a fixed number of triggers.
        if (mode == SimulationMode::FG_ONLY) {
            initial_triggers = 10; // A reasonable default for starting the FG sequence.
            result.fg_was_triggered = true;
        }

        // FULL_GAME mode involves picking a BG item first.
        if (mode == SimulationMode::FULL_GAME) {
            if (gameData.bg_items.empty()) return result;
            std::uniform_int_distribution<size_t> bg_dist(0, gameData.bg_items.size() - 1);
            const BG_Item& chosen_bg = gameData.bg_items[bg_dist(rng)];
            result.bg_score = chosen_bg.value;
            result.bg_levels = chosen_bg.levels;
            initial_triggers = chosen_bg.trigger_num;

            // Calculate max_bg_multiplier based on levels: {1→1, 2→2, 3→3, ≥4→5}
            if (chosen_bg.levels <= 0) {
                result.max_bg_multiplier = 1; // Safety: unexpected case, default to 1
            } else if (chosen_bg.levels == 1) {
                result.max_bg_multiplier = 1;
            } else if (chosen_bg.levels == 2) {
                result.max_bg_multiplier = 2;
            } else if (chosen_bg.levels == 3) {
                result.max_bg_multiplier = 3;
            } else { // levels >= 4
                result.max_bg_multiplier = 5;
            }

            // Apply the second chance probability if the initial trigger is zero.
            if (initial_triggers == 0 && second_chance_prob > 0) {
                std::uniform_real_distribution<double> chance_dist(0.0, 1.0);
                if (chance_dist(rng) < second_chance_prob) {
                    initial_triggers = 10; // Grant a single trigger on a successful second chance.
                }
            }
        }

        // --- Step 2: Process the FG sequence if triggered ---

        if (initial_triggers > 0) {
            result.fg_was_triggered = true;
            if (gameData.fg_items.empty()) return result; // No FG items to process

            std::vector<FG_Item> fg_processing_queue;
            fg_processing_queue.reserve(initial_triggers + 50); // Pre-allocate memory
            std::uniform_int_distribution<size_t> fg_dist(0, gameData.fg_items.size() - 1);

            // Add the initial items to the queue
            for (int i = 0; i < initial_triggers; ++i) {
                fg_processing_queue.push_back(gameData.fg_items[fg_dist(rng)]);
            }

            while (!fg_processing_queue.empty()) {
                // Safety check to prevent infinite loops and excess memory use
                if (fg_processing_queue.size() > MAX_QUEUE_SIZE) {
                    break;
                }

                result.fg_run_length++; // Count how many FG items are processed
                FG_Item current_fg = fg_processing_queue.back();
                fg_processing_queue.pop_back();

                // Track FG levels
                result.fg_levels.push_back(current_fg.levels);

                // Calculate multiplier based on FG item levels (for statistics tracking only)
                // The actual value already includes multiplier calculations
                // Mapping: {1→2, 2→4, 3→6, ≥4→10}
                long long fg_multiplier;
                if (current_fg.levels <= 0) {
                    fg_multiplier = 2; // Safety: unexpected case, default to 2
                } else if (current_fg.levels == 1) {
                    fg_multiplier = 2;
                } else if (current_fg.levels == 2) {
                    fg_multiplier = 4;
                } else if (current_fg.levels == 3) {
                    fg_multiplier = 6;
                } else { // levels >= 4
                    fg_multiplier = 10;
                }

                // Track max FG multiplier (statistics only)
                if (fg_multiplier > result.max_fg_multiplier) {
                    result.max_fg_multiplier = fg_multiplier;
                }

                // Add the value directly (already includes multiplier)
                result.fg_score += current_fg.value;

                // Track nonzero picks
                if (current_fg.value != 0) {
                    result.fg_nonzero_picks++;
                }

                // If the item has retriggers, add more items to the queue
                if (current_fg.retrigger_num > 0) {
                    for (int i = 0; i < current_fg.retrigger_num; ++i) {
                        fg_processing_queue.push_back(gameData.fg_items[fg_dist(rng)]);
                    }
                }
            }
        }

        return result;
    }

    /**
     * Provides safe, read-only access to the loaded game data.
     */
    const GameData& getGameData() {
        return gameData;
    }

} // namespace Game
