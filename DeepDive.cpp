#include "DeepDive.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <numeric>
#include <fstream>
#include "json.hpp"
#include <atomic> // For safety, include here as well

// Use the nlohmann namespace for convenience
using json = nlohmann::json;

namespace Game {

    // --- Internal Game Data Storage ---

    static DeepDiveData gameData;

    // --- THREAD-SAFE INITIALIZATION FIX ---
    // Use an atomic bool to prevent a race condition. This guarantees that when a
    // worker thread reads 'true', it also sees all the memory writes from the
    // JSON parsing that happened before the flag was set.
    static std::atomic<bool> isInitialized = false;

    // Safety limit to prevent a single game round from using too much memory.
    const size_t MAX_QUEUE_SIZE = 1000;

        const DeepDiveData& getGameData() {
        if (!isInitialized) {
            throw std::runtime_error("Attempted to getGameData() before initialization.");
        }
        return gameData;
    }


    // Helper to clear data before loading
    static void clearGameData() {
        gameData.bg_items.clear();
        gameData.fg_items.clear();
        gameData.multiplier_pools.clear();
        gameData.item_to_pool_map.clear();
        isInitialized = false;
    }

    void initializeWithSampleData() {
        clearGameData();
        std::cout << "Initializing DeepDive game data with hardcoded samples..." << std::endl;

        gameData.bg_items = {
            {101, 10, true, 1}, {102, 15, false, 2}, {103, 5, true, 1}
        };
        gameData.fg_items = {
            {201, 100, true, 3, 1}, {202, 250, false, 5, 2},
            {203, 500, true, 1, 3}, {204, 25, false, 0, 1},
            {205, 50, true, 4, 2}
        };
        gameData.multiplier_pools = {
            {1, 2, 3, 5, 10},
            {1, 1, 1, 3, 10}
        };
        gameData.item_to_pool_map = {
            {201, 0}, {202, 1}, {203, 1}, {205, 0}
        };

        isInitialized = true; // This write is now an atomic operation
        std::cout << "Sample data initialization complete." << std::endl;
    }
    
void initializeFromJSON(const std::string& filename, double bg_value_factor, double fg_value_factor) {
        clearGameData();
        std::cout << "Initializing DeepDive game data from '" << filename << "'..." << std::endl;
        if(bg_value_factor != 1.0) std::cout << "[Config] Applying BG value factor: " << bg_value_factor << std::endl;
        if(fg_value_factor != 1.0) std::cout << "[Config] Applying FG value factor: " << fg_value_factor << std::endl;

        std::ifstream file(filename);
        if (!file.is_open()) throw std::runtime_error("Could not open JSON file: " + filename);

        try {
            json data = json::parse(file);
            const auto& bg_items_json = data.at("bg_items");
            if (!bg_items_json.empty()) {
                if (bg_items_json[0].is_object()) {
                    for (const auto& item : bg_items_json) {
                        gameData.bg_items.push_back({
                            item.at("index").get<int>(),
                            static_cast<int>(item.at("value").get<double>() * bg_value_factor), // Apply factor and cast
                            item.at("flag").get<bool>(),
                            item.at("levels").get<int>()
                        });
                    }
                } else if (bg_items_json[0].is_array()) {
                    for (const auto& item_arr : bg_items_json) {
                        gameData.bg_items.push_back({
                            item_arr.at(0).get<int>(),
                            static_cast<int>(item_arr.at(1).get<double>() * bg_value_factor), // Apply factor and cast
                            item_arr.at(2).get<int>() == 1,
                            item_arr.at(3).get<int>()
                        });
                    }
                }
            }

            const auto& fg_items_json = data.at("fg_items");
            if (!fg_items_json.empty()) {
                if (fg_items_json[0].is_object()) {
                    for (const auto& item : fg_items_json) {
                        gameData.fg_items.push_back({
                            item.at("index").get<int>(),
                            static_cast<int>(item.at("value").get<double>() * fg_value_factor), // Apply factor and cast
                            item.at("flag").get<bool>(),
                            item.at("count").get<int>(),
                            item.at("levels").get<int>()
                        });
                    }
                } else if (fg_items_json[0].is_array()) {
                    for (const auto& item_arr : fg_items_json) {
                        gameData.fg_items.push_back({
                            item_arr.at(0).get<int>(),
                            static_cast<int>(item_arr.at(1).get<double>() * fg_value_factor), // Apply factor and cast
                            item_arr.at(2).get<int>() == 1,
                            item_arr.at(3).get<int>(),
                            item_arr.at(4).get<int>()
                        });
                    }
                }
            }
            
            gameData.multiplier_pools = data.at("multiplier_pools").get<std::vector<std::vector<long long>>>();
            for (auto& [key, val] : data.at("item_to_pool_map").items()) {
                gameData.item_to_pool_map[std::stoi(key)] = val.get<int>();
            }

        } catch (json::exception& e) {
            std::string error_msg = "JSON parsing error: ";
            error_msg += e.what();
            throw std::runtime_error(error_msg);
        }
        
        // --- Descriptive Statistics for Input Data ---
        std::cout << "\n------ Input Data Summary ------" << std::endl;

        // BG Items Stats
        long long bg_true_flags = 0;
        for(const auto& item : gameData.bg_items) {
            if (item.flag) bg_true_flags++;
        }
        double bg_trigger_prob = gameData.bg_items.empty() ? 0.0 : 100.0 * static_cast<double>(bg_true_flags) / gameData.bg_items.size();
        std::cout << "BG Items: " << gameData.bg_items.size() << " entries." << std::endl;
        std::cout << "  - Trigger Items (flag=true): " << bg_true_flags << " (" << std::fixed << std::setprecision(3) << bg_trigger_prob << "%)" << std::endl;

        long long bg_nonzero_values = 0;
        for(const auto& item : gameData.bg_items) {
            if (item.value != 0) bg_nonzero_values++;
        }
        double bg_nonzero_prob = gameData.bg_items.empty() ? 0.0 : 100.0 * static_cast<double>(bg_nonzero_values) / gameData.bg_items.size();
        std::cout << "  - Nonzero Values: " << bg_nonzero_values << " (" << std::fixed << std::setprecision(3) << bg_nonzero_prob << "%)" << std::endl;

        // BG Levels Stats
        // First verify data integrity: value == 0 should have levels == 1
        for(const auto& item : gameData.bg_items) {
            if (item.value == 0 && item.levels != 1) {
                std::cout << "  [Warning] BG Item index " << item.index
                          << " has value=0 but levels=" << item.levels << " (expected 1)" << std::endl;
            }
        }

        long long bg_total_levels = 0;
        long long bg_nonzero_value_count = 0;
        long long bg_nonzero_value_levels_sum = 0;
        int bg_max_level = 0;
        for(const auto& item : gameData.bg_items) {
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
        long long fg_true_flags = 0;
        for(const auto& item : gameData.fg_items) {
            if (item.flag) fg_true_flags++;
        }
        double fg_continue_prob = gameData.fg_items.empty() ? 0.0 : 100.0 * static_cast<double>(fg_true_flags) / gameData.fg_items.size();
        std::cout << "FG Items: " << gameData.fg_items.size() << " entries." << std::endl;
        std::cout << "  - Continue Items (flag=true): " << fg_true_flags << " (" << std::fixed << std::setprecision(3) << fg_continue_prob << "% chance per pick)" << std::endl;

        long long fg_nonzero_values = 0;
        for(const auto& item : gameData.fg_items) {
            if (item.value != 0) fg_nonzero_values++;
        }
        double fg_nonzero_prob = gameData.fg_items.empty() ? 0.0 : 100.0 * static_cast<double>(fg_nonzero_values) / gameData.fg_items.size();
        std::cout << "  - Nonzero Values: " << fg_nonzero_values << " (" << std::fixed << std::setprecision(3) << fg_nonzero_prob << "%)" << std::endl;

        // FG Levels Stats
        // First verify data integrity: value == 0 should have levels == 1
        for(const auto& item : gameData.fg_items) {
            if (item.value == 0 && item.levels != 1) {
                std::cout << "  [Warning] FG Item index " << item.index
                          << " has value=0 but levels=" << item.levels << " (expected 1)" << std::endl;
            }
        }

        long long fg_total_levels = 0;
        long long fg_nonzero_value_count = 0;
        long long fg_nonzero_value_levels_sum = 0;
        int fg_max_level = 0;
        for(const auto& item : gameData.fg_items) {
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

        // Multiplier Pool Stats
        std::cout << "Multiplier Pools:" << std::endl;
        for(size_t i = 0; i < gameData.multiplier_pools.size(); ++i) {
            const auto& pool = gameData.multiplier_pools[i];
            if(pool.empty()) {
                std::cout << "  - Pool ID " << i << ": Empty" << std::endl;
                continue;
            }
            long long sum = std::accumulate(pool.begin(), pool.end(), 0LL);
            double average = static_cast<double>(sum) / pool.size();
            std::cout << "  - Pool ID " << i << ": " << pool.size() << " values, Average Multiplier = " << std::fixed << std::setprecision(4) << average << std::endl;
        }
        std::cout << "--------------------------------" << std::endl;

        isInitialized = true;
        std::cout << "JSON data initialization complete." << std::endl;
    }

    GameResult simulateGameRound(std::mt19937& rng, SimulationMode mode, double second_chance_prob) {
        if (!isInitialized) {
            throw std::runtime_error("FATAL: Game logic called before data was initialized.");
        }
        if (gameData.bg_items.empty()) {
            return {0, 0, 0, false, 0, 1, 1, 0, {}};
        }

        // BG_Only Game process first
        if (mode == SimulationMode::BG_ONLY) {
            std::uniform_int_distribution<size_t> bg_dist(0, gameData.bg_items.size() - 1);
            const BG_Item& chosen_bg = gameData.bg_items[bg_dist(rng)];
            // Return only the BG score, with all FG stats as zero/false.
            return {static_cast<double>(chosen_bg.value), 0, 0, false, 0, 1, 1, chosen_bg.levels, {}};
        }


        double bg_score = 0.0;
        double fg_score = 0.0;
        long long fg_items_processed = 0;
        long long fg_nonzero_picks = 0;
        bool fg_was_triggered = false;
        bool proceed_to_fg = false;
        int bg_levels = 0;
        std::vector<int> fg_levels;

        if (mode == SimulationMode::FG_ONLY) {
            // In FG_ONLY mode, we skip BG logic entirely.
            // BG score is 0 and we always proceed.
            proceed_to_fg = true;
        } else { // FULL_GAME mode
            if (gameData.bg_items.empty()) return {0, 0, 0, false, 0, 1, 1, 0, {}};

            std::uniform_int_distribution<size_t> bg_dist(0, gameData.bg_items.size() - 1);
            const BG_Item& chosen_bg = gameData.bg_items[bg_dist(rng)];
            bg_score = chosen_bg.value;
            bg_levels = chosen_bg.levels;

            if (chosen_bg.flag) {
                proceed_to_fg = true;
            } else {
                // --- Second Chance Logic ---
                if (second_chance_prob > 0) {
                    std::uniform_real_distribution<double> chance_dist(0.0, 1.0);
                    if (chance_dist(rng) < second_chance_prob) {
                        proceed_to_fg = true;
                    }
                }
            }
        }

        if (!proceed_to_fg) {
            return {bg_score, 0, 0, false, 0, 1, 1, bg_levels, {}};
        }

        // --- FG Processing Stage ---
        long long max_fg_multiplier = 1;
        fg_was_triggered = true;
        if(gameData.fg_items.empty()) return {bg_score, 0, 0, true, 0, 1, 1, bg_levels, {}};

        std::vector<FG_Item> fg_processing_queue;
        fg_processing_queue.reserve(100);
        fg_levels.reserve(100);
        std::uniform_int_distribution<size_t> fg_dist(0, gameData.fg_items.size() - 1);

        for (int i = 0; i < 10; ++i) {
            fg_processing_queue.push_back(gameData.fg_items[fg_dist(rng)]);
        }

        // Added a flag to ensure the warning message only prints once per simulation run.
        static std::atomic<bool> cap_warning_logged_this_run = false;
        cap_warning_logged_this_run = false; // Reset for each new game round.


        while (!fg_processing_queue.empty()) {
            fg_items_processed++; // Increment counter for each item processed
            FG_Item current_fg = fg_processing_queue.back();
            fg_processing_queue.pop_back();
            fg_levels.push_back(current_fg.levels);

            long long total_multiplier;
            if (current_fg.count == 0) {
                total_multiplier = 1;
            } else {
                total_multiplier = 0;
                auto map_it = gameData.item_to_pool_map.find(current_fg.index);
                if (map_it != gameData.item_to_pool_map.end()) {
                    int pool_id = map_it->second;
                    if (pool_id >= 0 && pool_id < gameData.multiplier_pools.size()) {
                        const auto& pool = gameData.multiplier_pools[pool_id];
                        if (!pool.empty()) {
                            std::uniform_int_distribution<size_t> multi_dist(0, pool.size() - 1);
                            for (int i = 0; i < current_fg.count; ++i) {
                                total_multiplier += pool[multi_dist(rng)];
                            }
                        }
                    }
                }
            }

            double item_contribution = current_fg.value * total_multiplier;
            if (total_multiplier >= max_fg_multiplier) max_fg_multiplier = total_multiplier;
            fg_score += item_contribution;

            // Track nonzero picks
            if (item_contribution != 0.0) {
                fg_nonzero_picks++;
            }

            if (current_fg.flag) {
                if (fg_processing_queue.size() > MAX_QUEUE_SIZE) {
                    // Added a one-time warning message when the queue cap is hit.
                    bool already_logged = cap_warning_logged_this_run.exchange(true);
                    if (!already_logged) {
                        #pragma omp critical
                        {
                           std::cout << "\n[Warning] FG processing queue limit of " << MAX_QUEUE_SIZE << " reached. Capping round to prevent excess memory use.\n";
                        }
                    }
                    continue; // Memory protection cap
                }
                for (int i = 0; i < 10; ++i) {
                    fg_processing_queue.push_back(gameData.fg_items[fg_dist(rng)]);
                }
            }
        }
        return {bg_score, fg_score, fg_items_processed, true, fg_nonzero_picks, 1, max_fg_multiplier, bg_levels, fg_levels};
    }
}
