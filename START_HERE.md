# 🚀 START HERE - Monte Carlo Simulator

Welcome! This is your entry point for using the Monte Carlo Simulator.

---

## ⚡ Quick Start (30 seconds)

```bash
# 1. Build the simulator
./build_ss03.sh

# 2. Run it
./build/simulator
```

**That's it!** Your first simulation is running. ✅

---

## 📚 What to Read Next

Depending on what you need:

### **I want to run simulations right now** → [QUICK_REFERENCE.md](QUICK_REFERENCE.md)
One-page cheat sheet with all common commands.

### **I want to understand the full workflow** → [WALKTHROUGH.md](WALKTHROUGH.md)
Step-by-step guide covering everything from building to advanced usage.

### **I want to switch between games** → See below ⬇️

### **I want to understand the build system** → [BUILD.md](BUILD.md)
Technical details about CMake configuration.

### **I need complete documentation** → [README.md](README.md)
Full project documentation with all features explained.

---

## 🎮 Switching Between SS03Game and DeepDive

### Current Setup
- **Default game:** SS03Game
- **Default config:** `SS03_Merged_Flattened_Output.json`

### To Switch to DeepDive

**Step 1: Build for DeepDive**
```bash
./build_deepdive.sh
```

**Step 2: Update config file**
Open `MonteCarlo_main.cpp` and go to line ~115:

```cpp
// Change from:
const std::string configFile = "SS03_Merged_Flattened_Output.json";

// To:
const std::string configFile = "SS02_Config_Table01_v1.json";
```

**Step 3: Rebuild and run**
```bash
./build_deepdive.sh
./build/simulator
```

### To Switch Back to SS03Game

```bash
# 1. Build
./build_ss03.sh

# 2. Update config file back to SS03_Merged_Flattened_Output.json
# 3. Rebuild
./build_ss03.sh

# 4. Run
./build/simulator
```

---

## 🎯 Common Tasks

### Change Number of Simulations
Edit `MonteCarlo_main.cpp` line 90:
```cpp
const long long numSimulations = 100000000;  // Change this number
```
Then rebuild: `./build_ss03.sh`

### Use Different Memory Mode
Edit `MonteCarlo_main.cpp` line 159:
```cpp
MemoryMode::EFFICIENT   // For large runs (100M+)
MemoryMode::ACCURATE    // For exact results (< 100M)
```
Then rebuild.

### Save Results to File
```bash
./build/simulator > results.txt
```

---

## 🛠️ Using VS Code

### Build
1. `Cmd+Shift+P`
2. "Tasks: Run Build Task"
3. Choose your game

### Run
Press `F5` to run with debugger

### Edit Parameters
Open `MonteCarlo_main.cpp` and edit directly

---

## ❓ Troubleshooting

**Problem:** "array index 4 out of range"
```bash
# Solution: Rebuild
./build_ss03.sh
```

**Problem:** Wrong game is running
```bash
# Solution: Make sure you ran the correct build script
./build_ss03.sh  # for SS03Game
./build_deepdive.sh  # for DeepDive
```

**Problem:** Config file not found
```bash
# Solution: Check the config file path in MonteCarlo_main.cpp line ~115
# Make sure it matches a file that exists in your directory
```

---

## 📖 Documentation Files

| File | Purpose |
|------|---------|
| **START_HERE.md** | You are here! Entry point |
| **QUICK_REFERENCE.md** | One-page cheat sheet |
| **WALKTHROUGH.md** | Complete step-by-step guide |
| **BUILD.md** | Build system details |
| **SETUP_SUMMARY.md** | What we built and why |
| **README.md** | Full project documentation |

---

## 🎓 Key Concepts

### 1. Game Module = Compile Time
The game module (SS03Game or DeepDive) is selected when you **build**, not when you run.

### 2. Config File = Your Choice
You must update the config file path in `MonteCarlo_main.cpp` to match your game.

### 3. Always Rebuild
After editing any `.cpp` files, always rebuild before running.

---

## 🌟 Best Practices

✅ **Start with default settings**
- Run once with defaults to verify everything works

✅ **Test with small simulations first**
- Use 1,000,000 simulations for quick tests
- Scale up to 100,000,000+ for production

✅ **Save important results**
- Use `./simulator > results.txt` to save output

✅ **One game at a time**
- Don't try to switch games mid-workflow
- Complete your work with one game, then switch

---

## 🚀 Next Steps

1. ✅ Read this file (done!)
2. ✅ Run your first simulation (`./build_ss03.sh && ./build/simulator`)
3. 📖 Browse [QUICK_REFERENCE.md](QUICK_REFERENCE.md) for common commands
4. 🎓 Read [WALKTHROUGH.md](WALKTHROUGH.md) when you're ready to dive deeper

**Happy simulating!** 🎉

---

*For technical support, see the troubleshooting sections in WALKTHROUGH.md or BUILD.md*
