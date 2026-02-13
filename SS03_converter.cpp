/**
 * SS03_converter.cpp
 * 
 * Converts SS03 game data from individual JSON files to a flattened configuration file.
 * 
 * Input:  SS03 Data/BG_ReelSets/<folder>/<folder>_<id>.json
 *         SS03 Data/FG_ReelSets/<folder>/<folder>_<id>.json
 * 
 * Output: SS03_Config_Table01_v1.json
 * 
 * Each input JSON contains:
 *   - Payout: the value/payout
 *   - Free_Triggered: number of free games awarded (0, 10, 12, 14, etc.)
 *   - Steps: number of levels
 * 
 * Output format (both BG and FG use same structure):
 *   [index, value, trigger_num, levels]
 *   - index: assigned by read order (1 to 10000)
 *   - value: Payout
 *   - trigger_num: Free_Triggered value (actual FG count: 0, 10, 12, 14, etc.)
 *   - levels: Steps + 1 from JSON
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <map>
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Unified item structure for both BG and FG
struct GameItem {
    int index;
    int value;
    int trigger_num;  // Free_Triggered value (0, 10, 12, 14, etc.)
    int levels;       // Steps + 1
};

// Result structure to return items and scatter distribution
struct ProcessResult {
    std::vector<GameItem> items;
    std::map<int, int> scatter_distribution;  // Scatter_Count -> count (for printing only)
};

// Process all JSON files in a directory recursively
ProcessResult processDirectory(const std::string& basePath, const std::string& type) {
    ProcessResult result;
    
    if (!fs::exists(basePath)) {
        std::cerr << "[ERROR] Directory does not exist: " << basePath << std::endl;
        return result;
    }
    
    int errorCount = 0;
    int currentIndex = 1;  // Assign index based on read order (1 to 10000)
    
    // Collect all subdirectories and sort them numerically
    std::vector<fs::path> sortedDirs;
    for (const auto& dirEntry : fs::directory_iterator(basePath)) {
        if (!dirEntry.is_directory()) continue;
        std::string folderName = dirEntry.path().filename().string();
        if (folderName[0] == '.') continue;  // Skip hidden directories
        sortedDirs.push_back(dirEntry.path());
    }
    std::sort(sortedDirs.begin(), sortedDirs.end(), [](const fs::path& a, const fs::path& b) {
        return std::stoi(a.filename().string()) < std::stoi(b.filename().string());
    });
    
    // Iterate through sorted subdirectories
    for (const auto& dirPath : sortedDirs) {
        // Collect all JSON files in this directory and sort them
        std::vector<fs::path> sortedFiles;
        for (const auto& fileEntry : fs::directory_iterator(dirPath)) {
            if (!fileEntry.is_regular_file()) continue;
            std::string filename = fileEntry.path().filename().string();
            if (filename.find(".json") == std::string::npos) continue;
            sortedFiles.push_back(fileEntry.path());
        }
        
        // Sort files by the number after underscore (e.g., "0_123.json" -> 123)
        std::sort(sortedFiles.begin(), sortedFiles.end(), [](const fs::path& a, const fs::path& b) {
            std::string fnA = a.filename().string();
            std::string fnB = b.filename().string();
            size_t posA = fnA.find('_');
            size_t posB = fnB.find('_');
            int numA = (posA != std::string::npos) ? std::stoi(fnA.substr(posA + 1)) : 0;
            int numB = (posB != std::string::npos) ? std::stoi(fnB.substr(posB + 1)) : 0;
            return numA < numB;
        });
        
        // Process sorted files
        for (const auto& filePath : sortedFiles) {
            try {
                std::ifstream file(filePath);
                if (!file.is_open()) {
                    std::cerr << "[WARNING] Could not open: " << filePath << std::endl;
                    errorCount++;
                    continue;
                }
                
                json data = json::parse(file);
                file.close();
                
                GameItem item;
                item.index = currentIndex++;  // Assign index based on read order (1 to 10000)
                item.value = data.value("Payout", 0);
                item.trigger_num = data.value("Free_Triggered", 0);  // Actual FG count
                item.levels = data.value("Steps", 0) + 1;  // Use Steps + 1 for levels
                
                // Track scatter distribution (for printing only)
                int scatterCount = data.value("Scatter_Count", 0);
                result.scatter_distribution[scatterCount]++;
                
                result.items.push_back(item);
                
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse " << filePath << ": " << e.what() << std::endl;
                errorCount++;
            }
        }
    }
    
    std::cout << "  Processed " << result.items.size() << " files";
    if (errorCount > 0) {
        std::cout << " (" << errorCount << " errors)";
    }
    std::cout << std::endl;
    
    return result;
}

int main() {
    try {
        const std::string bgPath = "SS03 Data/BG_ReelSets";
        const std::string fgPath = "SS03 Data/FG_ReelSets";
        const std::string outputFile = "SS03_Config_Table01_v1.json";
        
        std::cout << "=== SS03 Data Converter ===" << std::endl;
        std::cout << std::endl;
        
        // Process BG items
        std::cout << "Processing Base Game (BG) items from: " << bgPath << std::endl;
        ProcessResult bg_result = processDirectory(bgPath, "BG");
        std::vector<GameItem>& bg_items = bg_result.items;
        std::cout << "  Found " << bg_items.size() << " unique BG items" << std::endl;
        
        // Process FG items
        std::cout << std::endl;
        std::cout << "Processing Free Game (FG) items from: " << fgPath << std::endl;
        ProcessResult fg_result = processDirectory(fgPath, "FG");
        std::vector<GameItem>& fg_items = fg_result.items;
        std::cout << "  Found " << fg_items.size() << " unique FG items" << std::endl;
        
        // Create output JSON
        json output;
        
        // Convert bg_items to compact array format [index, value, trigger_num, levels]
        json bg_array = json::array();
        for (const auto& item : bg_items) {
            bg_array.push_back({item.index, item.value, item.trigger_num, item.levels});
        }
        output["bg_items"] = bg_array;
        
        // Convert fg_items to compact array format [index, value, trigger_num, levels]
        json fg_array = json::array();
        for (const auto& item : fg_items) {
            fg_array.push_back({item.index, item.value, item.trigger_num, item.levels});
        }
        output["fg_items"] = fg_array;
        
        // Write output with custom formatting
        std::cout << std::endl;
        std::cout << "Writing output to: " << outputFile << std::endl;
        
        std::ofstream outFile(outputFile);
        if (!outFile.is_open()) {
            throw std::runtime_error("Could not open output file for writing: " + outputFile);
        }
        
        // Custom formatting: each item on one line
        outFile << "{\n";
        outFile << "    \"bg_items\": [\n";
        for (size_t i = 0; i < bg_array.size(); ++i) {
            outFile << "        " << bg_array[i].dump();
            if (i < bg_array.size() - 1) outFile << ",";
            outFile << "\n";
        }
        outFile << "    ],\n";
        outFile << "    \"fg_items\": [\n";
        for (size_t i = 0; i < fg_array.size(); ++i) {
            outFile << "        " << fg_array[i].dump();
            if (i < fg_array.size() - 1) outFile << ",";
            outFile << "\n";
        }
        outFile << "    ]\n";
        outFile << "}\n";
        outFile.close();
        
        // Print summary statistics
        std::cout << std::endl;
        std::cout << "=== Conversion Summary ===" << std::endl;
        
        // BG Statistics
        std::cout << std::endl;
        std::cout << "BG Items: " << bg_items.size() << " entries" << std::endl;
        
        int bg_triggers = 0;
        int bg_nonzero_values = 0;
        long long bg_total_triggers = 0;
        for (const auto& item : bg_items) {
            if (item.trigger_num > 0) {
                bg_triggers++;
                bg_total_triggers += item.trigger_num;
            }
            if (item.value != 0) bg_nonzero_values++;
        }
        std::cout << "  - Trigger Items (trigger_num > 0): " << bg_triggers
                  << " (" << (100.0 * bg_triggers / bg_items.size()) << "%)" << std::endl;
        if (bg_triggers > 0) {
            std::cout << "  - Avg Trigger Count: " << (double)bg_total_triggers / bg_triggers << std::endl;
        }
        std::cout << "  - Nonzero Values: " << bg_nonzero_values 
                  << " (" << (100.0 * bg_nonzero_values / bg_items.size()) << "%)" << std::endl;
        
        // BG Scatter Distribution from actual Scatter_Count field
        std::cout << "  - Scatter Distribution (from Scatter_Count field):" << std::endl;
        for (int sc = 0; sc <= 5; ++sc) {
            int count = 0;
            if (bg_result.scatter_distribution.count(sc)) {
                count = bg_result.scatter_distribution.at(sc);
            }
            std::cout << "      " << sc << " scatter: " << count 
                      << " (" << (100.0 * count / bg_items.size()) << "%)" << std::endl;
        }
        
        // FG Statistics
        std::cout << std::endl;
        std::cout << "FG Items: " << fg_items.size() << " entries" << std::endl;
        
        int fg_retriggers = 0;
        int fg_nonzero_values = 0;
        long long fg_total_retriggers = 0;
        for (const auto& item : fg_items) {
            if (item.trigger_num > 0) {
                fg_retriggers++;
                fg_total_retriggers += item.trigger_num;
            }
            if (item.value != 0) fg_nonzero_values++;
        }
        std::cout << "  - Retrigger Items (trigger_num > 0): " << fg_retriggers
                  << " (" << (100.0 * fg_retriggers / fg_items.size()) << "%)" << std::endl;
        if (fg_retriggers > 0) {
            std::cout << "  - Avg Retrigger Count: " << (double)fg_total_retriggers / fg_retriggers << std::endl;
        }
        std::cout << "  - Nonzero Values: " << fg_nonzero_values 
                  << " (" << (100.0 * fg_nonzero_values / fg_items.size()) << "%)" << std::endl;
        
        // FG Scatter Distribution from actual Scatter_Count field
        std::cout << "  - Scatter Distribution (from Scatter_Count field):" << std::endl;
        for (int sc = 0; sc <= 5; ++sc) {
            int count = 0;
            if (fg_result.scatter_distribution.count(sc)) {
                count = fg_result.scatter_distribution.at(sc);
            }
            std::cout << "      " << sc << " scatter: " << count 
                      << " (" << (100.0 * count / fg_items.size()) << "%)" << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "*** OUTPUT FORMAT ***" << std::endl;
        std::cout << "BG Items: [index, value, trigger_num, levels]" << std::endl;
        std::cout << "FG Items: [index, value, trigger_num, levels]" << std::endl;
        std::cout << std::endl;
        std::cout << "Conversion complete!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
