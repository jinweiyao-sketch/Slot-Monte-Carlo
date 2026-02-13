#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "json.hpp"

using json = nlohmann::json;

struct BGItem {
    int index;
    int value;
    int flag;
    int stop;
};

struct FGItem {
    int index;
    int value;
    int flag;
    int count;
    int stop;
};

// Count occurrences of a symbol in a board (6x5 grid)
int countSymbolInBoard(const std::vector<std::vector<int>>& board, int symbol) {
    int count = 0;
    for (const auto& row : board) {
        for (int cell : row) {
            if (cell == symbol) {
                count++;
            }
        }
    }
    return count;
}

// Get the first board from a script
std::vector<std::vector<int>> getFirstBoard(const std::vector<std::vector<std::vector<int>>>& script) {
    if (!script.empty()) {
        return script[0];
    }
    return {};
}

// Get the last board from a script
std::vector<std::vector<int>> getLastBoard(const std::vector<std::vector<std::vector<int>>>& script) {
    if (!script.empty()) {
        return script[script.size() - 1];
    }
    return {};
}

int main() {
    try {
        std::cout << "Reading SS02_script.json..." << std::endl;
        std::ifstream inputFile("SS02_script.json");
        if (!inputFile.is_open()) {
            throw std::runtime_error("Could not open SS02_script.json");
        }

        json data = json::parse(inputFile);
        inputFile.close();

        // Read multiplier_pools from moon.json
        json multiplier_pools = json::array();
        std::ifstream moonFile("moon.json");
        if (moonFile.is_open()) {
            try {
                json moonData = json::parse(moonFile);
                if (moonData.contains("data") && moonData["data"].contains("multiplier_table") 
                    && moonData["data"]["multiplier_table"].contains("free")) {
                    
                    const auto& freeMultipliers = moonData["data"]["multiplier_table"]["free"];
                    
                    // Process each pool (id 1 and id 2)
                    for (const auto& pool : freeMultipliers) {
                        if (!pool.contains("multiplier") || !pool.contains("weight")) {
                            continue;
                        }
                        
                        json poolArray = json::array();
                        const auto& multipliers = pool["multiplier"];
                        const auto& weights = pool["weight"];
                        
                        // Expand multipliers according to weights
                        for (size_t i = 0; i < multipliers.size() && i < weights.size(); ++i) {
                            int mult = multipliers[i].get<int>();
                            int weight = weights[i].get<int>();
                            
                            // Convert from 1xx format to actual multiplier (102 -> 2, 103 -> 3, etc.)
                            int actualMult = mult - 100;
                            
                            // Add multiplier 'weight' times
                            for (int w = 0; w < weight; ++w) {
                                poolArray.push_back(actualMult);
                            }
                        }
                        
                        multiplier_pools.push_back(poolArray);
                    }
                    
                    std::cout << "Read multiplier_pools from moon.json" << std::endl;
                } else {
                    std::cerr << "[WARNING] moon.json does not contain expected multiplier_table structure" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[WARNING] Could not parse moon.json: " << e.what() << std::endl;
            }
            moonFile.close();
        } else {
            std::cerr << "[WARNING] Could not open moon.json, no multiplier pools will be added" << std::endl;
        }

        std::vector<BGItem> bg_items;
        std::vector<FGItem> fg_items;
        json item_to_pool_map = json::object();

        // Process base game scripts
        std::cout << "Processing base game scripts with stop information..." << std::endl;
        std::vector<int> invalid_bg_indices;
        if (data.contains("base")) {
            const auto& baseScripts = data["base"];
            
            for (size_t idx = 0; idx < baseScripts.size(); ++idx) {
                const auto& scriptEntry = baseScripts[idx];
                
                // Warn if entries without index or payout (incomplete entries)
                if (!scriptEntry.contains("index") || !scriptEntry.contains("payout")) {
                    std::cerr << "[WARNING] Base game entry at position " << idx << " is missing index or payout field" << std::endl;
                    continue;
                }
                
                int index = scriptEntry["index"].get<int>();
                int payout = scriptEntry["payout"].get<int>();
                int stop = scriptEntry.contains("stop") ? scriptEntry["stop"].get<int>() : 0;
                
                // Warn if script is empty
                if (scriptEntry.contains("script") && scriptEntry["script"].empty()) {
                    invalid_bg_indices.push_back(index);
                    bg_items.push_back({index, payout, 0, stop});
                    continue;
                }
                
                // Skip if no script field
                if (!scriptEntry.contains("script")) {
                    continue;
                }
                
                // Get script data (array of boards)
                const auto& script = scriptEntry["script"].get<std::vector<std::vector<std::vector<int>>>>();
                
                // Count SC symbols (201) in first board
                auto firstBoard = getFirstBoard(script);
                int scCount = countSymbolInBoard(firstBoard, 201);
                
                // Flag is 1 if at least 4 SC symbols, else 0
                int flag = (scCount >= 4) ? 1 : 0;
                
                bg_items.push_back({index, payout, flag, stop});
            }
        }

        // Process free game scripts
        std::cout << "Processing free game scripts with stop information..." << std::endl;
        std::vector<int> invalid_fg_indices;
        if (data.contains("free")) {
            const auto& freeScripts = data["free"];
            
            for (size_t idx = 0; idx < freeScripts.size(); ++idx) {
                const auto& scriptEntry = freeScripts[idx];
                
                // Warn if entries without index or payout (incomplete entries)
                if (!scriptEntry.contains("index") || !scriptEntry.contains("payout")) {
                    std::cerr << "[WARNING] Free game entry at position " << idx << " is missing index or payout field" << std::endl;
                    continue;
                }
                
                int index = scriptEntry["index"].get<int>();
                int payout = scriptEntry["payout"].get<int>();
                int stop = scriptEntry.contains("stop") ? scriptEntry["stop"].get<int>() : 0;
                
                // Warn if script is empty
                if (scriptEntry.contains("script") && scriptEntry["script"].empty()) {
                    invalid_fg_indices.push_back(index);
                    fg_items.push_back({index, payout, 0, 0, stop});
                    continue;
                }
                
                // Skip if no script field
                if (!scriptEntry.contains("script")) {
                    continue;
                }
                
                // Get script data (array of boards)
                const auto& script = scriptEntry["script"].get<std::vector<std::vector<std::vector<int>>>>();
                
                // Count SC symbols (201) in first board for flag
                auto firstBoard = getFirstBoard(script);
                int scCountFirst = countSymbolInBoard(firstBoard, 201);
                int flag = (scCountFirst >= 3) ? 1 : 0;
                
                // Count multiplier symbols (202) in last board
                auto lastBoard = getLastBoard(script);
                int counted202 = countSymbolInBoard(lastBoard, 202);
                
                // Get multiplier_count from JSON if available, otherwise use counted value
                int multiplierCount = counted202;
                if (scriptEntry.contains("multiplier_count")) {
                    int jsonMultiplierCount = scriptEntry["multiplier_count"].get<int>();
                    // Verify: warn if different
                    if (jsonMultiplierCount != counted202) {
                        std::cerr << "[WARNING] FG item index " << index << ": multiplier_count in JSON (" 
                                  << jsonMultiplierCount << ") differs from counted 202 symbols (" 
                                  << counted202 << ")" << std::endl;
                    }
                    multiplierCount = jsonMultiplierCount;
                }
                
                // Adjust payout if special_multipliers > 1
                int adjustedPayout = payout;
                if (scriptEntry.contains("special_multipliers")) {
                    int special_mult = scriptEntry["special_multipliers"].get<int>();
                    if (special_mult > 1 && multiplierCount > 0) {
                        adjustedPayout = payout / (special_mult * multiplierCount);
                    }
                }
                
                fg_items.push_back({index, adjustedPayout, flag, multiplierCount, stop});
                
                // Map item to pool based on special_multipliers
                int pool_id = 0;  // Default pool
                if (scriptEntry.contains("special_multipliers")) {
                    int special_mult = scriptEntry["special_multipliers"].get<int>();
                    if (special_mult == 20) {
                        pool_id = 1;
                    }
                }
                item_to_pool_map[std::to_string(index)] = pool_id;
            }
        }

        // Create output JSON
        json output;
        
        // Convert bg_items to compact array format with stop field
        json bg_array = json::array();
        for (const auto& item : bg_items) {
            bg_array.push_back({item.index, item.value, item.flag, item.stop});
        }
        output["bg_items"] = bg_array;

        // Convert fg_items to compact array format with stop field
        json fg_array = json::array();
        for (const auto& item : fg_items) {
            fg_array.push_back({item.index, item.value, item.flag, item.count, item.stop});
        }
        output["fg_items"] = fg_array;

        // Add multiplier_pools from moon.json
        if (!multiplier_pools.is_null() && multiplier_pools.is_array() && !multiplier_pools.empty()) {
            output["multiplier_pools"] = multiplier_pools;
        }

        // Add item_to_pool_map
        output["item_to_pool_map"] = item_to_pool_map;

        // Write output
        std::cout << "Writing output to SS02_Config_Table01_v1.json..." << std::endl;
        std::ofstream outputFile("SS02_Config_Table01_v1.json");
        if (!outputFile.is_open()) {
            throw std::runtime_error("Could not open output file for writing");
        }

        // Custom formatting: each item on one line
        outputFile << "{\n";
        outputFile << "  \"bg_items\": [\n";
        for (size_t i = 0; i < bg_array.size(); ++i) {
            outputFile << "    " << bg_array[i].dump() << (i < bg_array.size() - 1 ? "," : "") << "\n";
        }
        outputFile << "  ],\n";
        outputFile << "  \"fg_items\": [\n";
        for (size_t i = 0; i < fg_array.size(); ++i) {
            outputFile << "    " << fg_array[i].dump() << (i < fg_array.size() - 1 ? "," : "") << "\n";
        }
        outputFile << "  ],\n";
        
        // Write multiplier_pools if they exist
        if (!multiplier_pools.is_null() && multiplier_pools.is_array() && !multiplier_pools.empty()) {
            outputFile << "  \"multiplier_pools\": [\n";
            for (size_t pool_idx = 0; pool_idx < multiplier_pools.size(); ++pool_idx) {
                outputFile << "    [";
                const auto& pool = multiplier_pools[pool_idx];
                for (size_t i = 0; i < pool.size(); ++i) {
                    outputFile << pool[i].get<int>();
                    if (i < pool.size() - 1) {
                        outputFile << ",";
                    }
                }
                outputFile << "]" << (pool_idx < multiplier_pools.size() - 1 ? "," : "") << "\n";
            }
            outputFile << "  ],\n";
        }
        
        outputFile << "  \"item_to_pool_map\": {\n";
        
        // Convert map to vector of pairs and sort numerically by index
        std::vector<std::pair<int, int>> sorted_items;
        for (const auto& [key, val] : item_to_pool_map.items()) {
            sorted_items.push_back({std::stoi(key), val.get<int>()});
        }
        std::sort(sorted_items.begin(), sorted_items.end());
        
        // Format item_to_pool_map with 10 items per line
        for (size_t i = 0; i < sorted_items.size(); ++i) {
            if (i % 10 == 0) {
                outputFile << "    ";
            }
            outputFile << "\"" << sorted_items[i].first << "\": " << sorted_items[i].second;
            if (i < sorted_items.size() - 1) {
                outputFile << ", ";
                if ((i + 1) % 10 == 0) {
                    outputFile << "\n";
                }
            }
        }
        outputFile << "\n  }\n";
        outputFile << "}\n";
        outputFile.close();

        // Print summary
        std::cout << "\n------ Conversion Summary ------" << std::endl;
        std::cout << "BG Items (Base Game Scripts): " << bg_items.size() << std::endl;
        int bg_triggers = 0;
        for (const auto& item : bg_items) {
            if (item.flag == 1) bg_triggers++;
        }
        std::cout << "  - Trigger Items (flag=1): " << bg_triggers << std::endl;

        std::cout << "FG Items (Free Game Scripts): " << fg_items.size() << std::endl;
        int fg_continues = 0;
        for (const auto& item : fg_items) {
            if (item.flag == 1) fg_continues++;
        }
        std::cout << "  - Continue Items (flag=1): " << fg_continues << std::endl;

        if (!multiplier_pools.is_null() && multiplier_pools.is_array() && !multiplier_pools.empty()) {
            std::cout << "\nMultiplier Pools: Read from moon.json" << std::endl;
            std::cout << "  - " << multiplier_pools.size() << " pool(s) found" << std::endl;
        } else {
            std::cout << "\nNote: No multiplier_pools found in moon.json." << std::endl;
            std::cout << "      Please check moon.json or configure multiplier_pools manually." << std::endl;
        }
        std::cout << "Item-to-Pool Map: " << item_to_pool_map.size() << " FG items mapped" << std::endl;

        std::cout << "\n*** OUTPUT FORMAT ***" << std::endl;
        std::cout << "BG Items: [index, value, flag, stop]" << std::endl;
        std::cout << "FG Items: [index, value, flag, count, stop]" << std::endl;

        std::cout << "\nConversion complete!" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
