# Monte Carlo Simulator for Slot Game Analysis

A high-performance C++ Monte Carlo simulator for analyzing the statistical properties, volatility, and return-to-player (RTP) characteristics of complex slot-style games with configurable mechanics.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![CMake](https://img.shields.io/badge/CMake-3.15+-064F8C.svg)](https://cmake.org/)
[![OpenMP](https://img.shields.io/badge/OpenMP-Parallel-brightgreen.svg)](https://www.openmp.org/)

---

## 🚀 Quick Start

**New users should start with [START_HERE.md](START_HERE.md)** for a guided introduction.

```bash
# 1. Build the simulator
./build_ss03.sh

# 2. Run it
./build/simulator
```

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Project Structure](#project-structure)
4. [Building the Project](#building-the-project)
5. [Running Simulations](#running-simulations)
6. [Configuration](#configuration)
7. [Understanding the Output](#understanding-the-output)
8. [Game Modules](#game-modules)
9. [Documentation](#documentation)
10. [Requirements](#requirements)

---

## Overview

This simulator performs Monte Carlo analysis on slot-style games where:
- A **Base Game (BG)** round randomly selects one item from a pool
- The BG item may trigger a **Free Game (FG)** feature with cascading selections
- Each item can have multipliers and levels that affect payouts
- The simulator tracks comprehensive statistics including RTP, volatility, confidence intervals, and distribution analysis

### Key Features

- **Dual Game Module Support**:
  - **SS03Game**: Trigger-based game with retrigger mechanics
  - **DeepDive**: Multiplier pool-based game with complex cascading
- **Parallel Processing**: Multi-threaded execution using OpenMP for high performance
- **Dual Memory Modes**:
  - **Efficient Mode**: Low memory (~100 MB), online statistics computation
  - **Accurate Mode**: Full data storage for exact percentile analysis
- **Confidence Intervals**: Statistical confidence bounds using batched means or bootstrap methods
- **Comprehensive Statistics**: Mean, variance, skewness, kurtosis, percentiles, histogram distributions
- **Flexible Configuration**: JSON-based game configuration with value scaling factors
- **VS Code Integration**: Pre-configured build tasks and debug configurations

---

## Project Structure

```
SlotMC/
├── MonteCarlo_main.cpp         # Main entry point and configuration
├── MonteCarloSimulator.{h,cpp} # Core simulation engine
├── Statistics.{h,cpp}          # Statistical analysis functions
├── GameModule.h                # Automatic game module selector
├── SS03Game.{h,cpp}            # SS03Game implementation
├── DeepDive.{h,cpp}            # DeepDive implementation
├── CMakeLists.txt              # CMake build configuration
├── build_ss03.sh               # Quick build script for SS03Game
├── build_deepdive.sh           # Quick build script for DeepDive
├── .vscode/                    # VS Code configuration
│   ├── launch.json             # Debug configuration
│   └── tasks.json              # Build tasks
├── START_HERE.md               # 🌟 Start here for new users
├── QUICK_REFERENCE.md          # One-page command reference
├── WALKTHROUGH.md              # Detailed step-by-step guide
├── BUILD.md                    # Build system details
└── *.json                      # Game configuration files
```

---

## Building the Project

### Prerequisites

- **C++17** compatible compiler
- **CMake 3.15+**
- **OpenMP** support
- **g++-15** (macOS with Homebrew) or equivalent

**macOS Setup:**
```bash
brew install libomp
brew install gcc@15
```

### Build Methods

#### Method 1: Build Scripts (Recommended)

**For SS03Game:**
```bash
./build_ss03.sh
```

**For DeepDive:**
```bash
./build_deepdive.sh
```

The executable will be at `./build/simulator`

#### Method 2: CMake Directly

```bash
# For SS03Game
cmake -B build -DGAME_MODULE=SS03Game -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15
cmake --build build -j 10

# For DeepDive
cmake -B build -DGAME_MODULE=DeepDive -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15
cmake --build build -j 10
```

#### Method 3: VS Code

1. Press `Cmd+Shift+P` (macOS) or `Ctrl+Shift+P` (Windows/Linux)
2. Select **Tasks: Run Build Task**
3. Choose **Build Project Simulator (SS03Game)** or **(DeepDive)**
4. Press `F5` to run/debug

---

## Running Simulations

```bash
# Run the simulator
./build/simulator

# Save results to file
./build/simulator > results.txt

# Quick test (edit numSimulations in MonteCarlo_main.cpp first)
./build/simulator | head -100
```

---

## Configuration

All configuration is done in `MonteCarlo_main.cpp`:

### Key Parameters

```cpp
const int base_bet = 20;                        // Base bet amount
const long long numSimulations = 100000000;     // Total simulation rounds
const long long batches = 100;                  // Number of batches
const std::string configFile = "SS03_Merged_Flattened_Output.json";
const bool useParallel = true;                  // Enable parallel processing
```

### Simulation Modes

```cpp
const Game::SimulationMode sim_mode = Game::SimulationMode::FULL_GAME;
```

**Options:**
- `FULL_GAME`: Complete game simulation (BG + FG)
- `FG_ONLY`: Free game only (auto-triggered)
- `BG_ONLY`: Base game only (no FG triggers)

### Memory Modes

Choose in the `simulator.run()` call:
- `MemoryMode::EFFICIENT`: Low memory (~100 MB), good for 100M+ simulations
- `MemoryMode::ACCURATE`: High memory (~77 bytes per sim), exact percentiles

### Value Scaling

```cpp
const double bg_value_factor = 1.0;  // Scale BG values (1.0 = no change)
const double fg_value_factor = 1.0;  // Scale FG values (1.0 = no change)
```

---

## Understanding the Output

The simulator provides comprehensive output including:

### Summary Statistics
- **RTP (Return to Player)**: Expected return as a percentage of bet
- **Volatility**: Standard deviation of returns
- **Confidence Intervals**: 95% confidence bounds for RTP

### Distribution Analysis
- **Mean, Median, Mode**
- **Skewness and Kurtosis**
- **Percentiles**: P1, P5, P10, P25, P50, P75, P90, P95, P99
- **Histogram**: Distribution across custom bins

### Game-Specific Metrics
- **FG Trigger Rate**: How often free games are triggered
- **FG Run Length**: Average number of picks in free games
- **Max Multipliers**: Observed maximum multipliers (BG and FG)
- **Levels Distribution**: Analysis of level mechanics

---

## Game Modules

### SS03Game
- **Mechanics**: Trigger-based with retrigger support
- **JSON Format**: Compact arrays `[index, value, trigger_num, levels]`
- **Configuration**: `SS03_Merged_Flattened_Output.json`
- **Features**:
  - Trigger distribution analysis
  - Retrigger distribution analysis
  - Dynamic multipliers based on levels

### DeepDive
- **Mechanics**: Multiplier pools with cascading
- **JSON Format**: Complex objects with multiplier pools and mappings
- **Configuration**: `SS02_Config_Table01_v1.json`
- **Features**:
  - Random multiplier selection from pools
  - Item-to-pool mapping system
  - Flag-based triggering

### Switching Between Games

1. Build with the desired game module:
   ```bash
   ./build_ss03.sh  # or ./build_deepdive.sh
   ```

2. Update config file in `MonteCarlo_main.cpp` (line ~115):
   ```cpp
   const std::string configFile = "SS03_Merged_Flattened_Output.json"; // SS03Game
   // const std::string configFile = "SS02_Config_Table01_v1.json";    // DeepDive
   ```

3. Rebuild and run:
   ```bash
   ./build_ss03.sh  # or ./build_deepdive.sh
   ./build/simulator
   ```

---

## Documentation

| File | Purpose |
|------|---------|
| **[START_HERE.md](START_HERE.md)** | 🌟 Entry point for new users |
| **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** | One-page command cheat sheet |
| **[WALKTHROUGH.md](WALKTHROUGH.md)** | Complete step-by-step guide |
| **[BUILD.md](BUILD.md)** | Detailed build system information |
| **[SETUP_SUMMARY.md](SETUP_SUMMARY.md)** | Project setup and architecture |
| **README.md** | This file - project overview |

---

## Requirements

### Software
- **C++ Compiler**:
  - GCC 7+ with C++17 support
  - Clang 5+ with C++17 support
  - MSVC 2017+ with C++17 support
- **CMake**: 3.15 or higher
- **OpenMP**: For parallel processing support

### Hardware Recommendations
- **CPU**: Multi-core processor (8+ cores recommended for parallel mode)
- **Memory**:
  - Efficient mode: 2 GB minimum
  - Accurate mode: 8 GB+ for 100M simulations, 80 GB for 1B simulations
- **Storage**: Minimal (< 100 MB for project + build artifacts)

### Platform Support
- ✅ macOS (tested on Apple Silicon and Intel)
- ✅ Linux (tested on Ubuntu 20.04+)
- ⚠️ Windows (should work with MSVC but not extensively tested)

---

## Performance

Typical performance on modern hardware:

| Simulation Count | Mode | Threads | Time | Memory |
|-----------------|------|---------|------|--------|
| 100M | Efficient | 10 | ~2-5 min | ~100 MB |
| 100M | Accurate | 10 | ~3-6 min | ~7.7 GB |
| 1B | Efficient | 10 | ~20-50 min | ~100 MB |

*Performance varies based on game complexity, FG trigger rates, and hardware*

---

## Troubleshooting

### Common Issues

**"Could NOT find OpenMP" error:**
```bash
# macOS
brew install libomp

# Then rebuild
./build_ss03.sh
```

**"array index out of range" error:**
```bash
# You're running an old binary, rebuild:
./build_ss03.sh
```

**Wrong game module loaded:**
- Make sure you built with the correct script
- Check that config file matches the game module

**Debug task not working in VS Code:**
- Ensure you built with a build task first
- Check that `./build/simulator` exists

---

## License

This project is provided as-is for educational and research purposes.

---

## Contributing

For questions, issues, or contributions, please refer to the documentation files or contact the project maintainer.

---

**Happy Simulating!** 🎰📊
