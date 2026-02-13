# Complete Walkthrough - How to Use the Monte Carlo Simulator

This guide walks you through everything from building to running simulations and switching between game modules.

---

## 📋 Table of Contents

1. [Quick Start (First Time)](#quick-start-first-time)
2. [Running Simulations](#running-simulations)
3. [Switching Between Games](#switching-between-games)
4. [Configuring Simulations](#configuring-simulations)
5. [Using VS Code](#using-vs-code)
6. [Common Workflows](#common-workflows)

---

## 🚀 Quick Start (First Time)

### Step 1: Navigate to Project Directory
```bash
cd /Users/haoxingliu/Documents/C/SlotMC
```

### Step 2: Build for Your Desired Game

**For SS03Game:**
```bash
./build_ss03.sh
```

**For DeepDive:**
```bash
./build_deepdive.sh
```

You'll see output like:
```
======================================
Building Monte Carlo Simulator
Game Module: SS03Game
======================================
-- Building with game module: SS03Game
...
[100%] Built target simulator
Build complete! Run with: ./build/simulator
```

### Step 3: Run the Simulator
```bash
./build/simulator
```

That's it! The simulation will run and display results.

---

## 🎯 Running Simulations

### Basic Execution

After building, simply run:
```bash
./build/simulator
```

### What You'll See

1. **Configuration Loading:**
   ```
   Initializing SS03Game data from 'SS03_Merged_Flattened_Output.json'...
   BG Items: 100000 entries.
   FG Items: 100000 entries.
   ```

2. **Simulation Progress:**
   ```
   [Monitor] Running in PARALLEL mode.
   Progress: Batch 10/100 (10.0% complete)
   Progress: Batch 20/100 (20.0% complete)
   ...
   ```

3. **Results:**
   ```
   ------ Monte Carlo Simulation Results ------
   Simulations Run:   100000000
   Mean:              96.543210
   RTP:               482.7161%
   ...
   ```

### Stopping a Running Simulation

Press `Ctrl+C` to interrupt.

---

## 🔄 Switching Between Games

### The Key Concept

**Important:** The game module is selected when you BUILD, not when you run!

You CANNOT switch games by editing code - you must rebuild with a different game module.

### Complete Switch Workflow

Let's say you're currently using SS03Game and want to switch to DeepDive:

#### Step 1: Build for DeepDive
```bash
./build_deepdive.sh
```

#### Step 2: Update Config File in MonteCarlo_main.cpp

Open `MonteCarlo_main.cpp` and change line ~115:

**Before:**
```cpp
const std::string configFile = "SS03_Merged_Flattened_Output.json";
```

**After:**
```cpp
// const std::string configFile = "SS03_Merged_Flattened_Output.json";
const std::string configFile = "SS02_Config_Table01_v1.json";
```

#### Step 3: Rebuild
```bash
./build_deepdive.sh
```

#### Step 4: Run
```bash
./build/simulator
```

### Switching Back to SS03Game

Just reverse the process:

```bash
./build_ss03.sh
```

Then update config file back to SS03, rebuild, and run.

### Quick Reference

| Game Module | Build Command | Config File Example |
|-------------|---------------|---------------------|
| **SS03Game** | `./build_ss03.sh` | `SS03_Merged_Flattened_Output.json` |
| **DeepDive** | `./build_deepdive.sh` | `SS02_Config_Table01_v1.json` |

---

## ⚙️ Configuring Simulations

All simulation parameters are in `MonteCarlo_main.cpp`.

### Key Parameters to Adjust

Open `MonteCarlo_main.cpp` and look for:

#### 1. Number of Simulations
```cpp
const long long numSimulations = 100000000;  // Total rounds
const long long batches = 100;               // Number of batches
```

**Common configurations:**
- **Quick test:** 1,000,000 total (10 batches × 100,000)
- **Normal run:** 100,000,000 total (100 batches × 1,000,000)
- **Production:** 1,000,000,000 total (1000 batches × 1,000,000)

#### 2. Memory Mode
```cpp
simulator.run(batches, batch_rounds, sim_mode,
              MemoryMode::EFFICIENT,  // Change to ACCURATE if needed
              useParallel, second_chance_prob);
```

**Choose:**
- `MemoryMode::EFFICIENT` - For large runs (100M+)
- `MemoryMode::ACCURATE` - For exact percentiles (< 100M)

#### 3. Simulation Mode
```cpp
const Game::SimulationMode sim_mode = Game::SimulationMode::FULL_GAME;
```

**Options:**
- `FULL_GAME` - Complete game (BG + FG)
- `FG_ONLY` - Only Free Game rounds
- `BG_ONLY` - Only Base Game rounds

#### 4. Config File
```cpp
const std::string configFile = "SS03_Merged_Flattened_Output.json";
```

Must match your game module!

### After Making Changes

Always rebuild:
```bash
./build_ss03.sh   # or ./build_deepdive.sh
```

---

## 💻 Using VS Code

### Building in VS Code

1. **Open Command Palette:** `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows/Linux)
2. **Type:** "Tasks: Run Build Task"
3. **Select:**
   - `Build Project Simulator (SS03Game)` - Builds for SS03Game
   - `Build Project Simulator (DeepDive)` - Builds for DeepDive

### Running in VS Code

**Method 1: Terminal**
1. Open integrated terminal: ``Ctrl+` ``
2. Run: `./build/simulator`

**Method 2: Debugger**
1. Press `F5` (or Debug → Start Debugging)
2. Simulator runs with debugger attached

### Debugging

Set breakpoints and press `F5`:
- Breakpoints work in all `.cpp` files
- Variables are inspectable
- Can step through code

### Current Game Module

Check the bottom of VS Code - you'll see which task ran last.

Or check the build output:
```
-- Building with game module: SS03Game
```

---

## 🔧 Common Workflows

### Workflow 1: Testing a New Configuration

```bash
# 1. Edit config file parameters in JSON
vim SS03_Merged_Flattened_Output.json

# 2. Build
./build_ss03.sh

# 3. Run quick test (edit MonteCarlo_main.cpp to use fewer simulations)
./build/simulator

# 4. If good, increase simulations and run full test
./build/simulator > results.txt
```

### Workflow 2: Comparing Two Games

```bash
# Run SS03Game
./build_ss03.sh
./build/simulator > results_ss03.txt

# Switch to DeepDive
./build_deepdive.sh
# Edit config file in MonteCarlo_main.cpp
./build/simulator > results_deepdive.txt

# Compare results
diff results_ss03.txt results_deepdive.txt
```

### Workflow 3: Parameter Sweep

```bash
# Create a script to test different parameters
for sim_count in 1000000 10000000 100000000; do
    # Edit MonteCarlo_main.cpp to set numSimulations
    ./build_ss03.sh
    ./build/simulator > results_${sim_count}.txt
done
```

### Workflow 4: Clean Rebuild (Troubleshooting)

```bash
# Remove all build artifacts
rm -rf build simulator

# Rebuild from scratch
./build_ss03.sh

# Run
./build/simulator
```

---

## 📁 File Organization

```
SlotMC/
├── MonteCarlo_main.cpp        # ← Edit parameters here
├── build_ss03.sh              # ← Run this for SS03Game
├── build_deepdive.sh          # ← Run this for DeepDive
├── simulator                  # ← Built executable (run this)
├── SS03_Merged_*.json         # ← SS03Game config files
├── SS02_Config_*.json         # ← DeepDive config files
├── build/                     # ← Build artifacts (auto-generated)
└── .vscode/                   # ← VS Code configuration
    └── tasks.json             # ← Build tasks
```

---

## ❓ FAQ

**Q: I see red squiggles in VS Code but the code compiles fine. What's wrong?**
A: IntelliSense needs to know which game module is active. See `.vscode/README_INTELLISENSE.md` for how to update it.

**Q: Do I need to clean the build directory when switching games?**
A: No! Just run the appropriate build script.

**Q: Can I have both SS03Game and DeepDive executables?**
A: Not simultaneously. The `simulator` executable is overwritten each build. You could rename it:
```bash
./build_ss03.sh
cp simulator simulator_ss03
./build_deepdive.sh
cp simulator simulator_deepdive
```

**Q: How do I know which game module I built with?**
A: Run the simulator - it prints the config file it's loading, which tells you the game.

**Q: The config file path is wrong - how do I fix it?**
A: Edit `MonteCarlo_main.cpp` line ~115, rebuild, and run.

**Q: Can I run simulations in parallel on multiple machines?**
A: Yes! Copy the entire directory to each machine and run independently.

**Q: How long does a simulation take?**
A: Depends on simulation count and CPU:
- 1M simulations: ~1 second
- 100M simulations: ~2 minutes
- 1B simulations: ~20 minutes
(on a 10-core machine with parallel mode enabled)

---

## 🎓 Summary

**To run a simulation:**
1. `./build_ss03.sh` (or `build_deepdive.sh`)
2. `./build/simulator`

**To switch games:**
1. Run different build script
2. Update config file in MonteCarlo_main.cpp
3. Rebuild
4. Run

**To change parameters:**
1. Edit MonteCarlo_main.cpp
2. Rebuild
3. Run

**That's it!** Everything else is automatic. 🎉

---

For more technical details:
- **BUILD.md** - Detailed build documentation
- **SETUP_SUMMARY.md** - Build system overview
- **README.md** - Complete project documentation
