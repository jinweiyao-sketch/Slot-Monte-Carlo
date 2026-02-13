# Quick Reference Card

## 🎯 Most Common Commands

### Build and Run
```bash
# Build for SS03Game (default)
./build_ss03.sh

# Run simulation
./build/simulator

# Build for DeepDive
./build_deepdive.sh
```

### In VS Code
```
Cmd+Shift+P → "Tasks: Run Build Task" → Choose game
F5 → Run/Debug
```

---

## 🔄 Switching Games

**Step 1: Build**
```bash
./build_deepdive.sh  # or ./build_ss03.sh
```

**Step 2: Edit Config (MonteCarlo_main.cpp line ~115)**
```cpp
// SS03Game:
const std::string configFile = "SS03_Merged_Flattened_Output.json";

// DeepDive:
const std::string configFile = "SS02_Config_Table01_v1.json";
```

**Step 3: Rebuild and Run**
```bash
./build_deepdive.sh
./build/simulator
```

---

## 📝 Quick Configuration Guide

### Location
Edit: `MonteCarlo_main.cpp`

### Key Parameters

| Parameter | Line | Common Values |
|-----------|------|---------------|
| **Simulations** | ~90 | `1000000`, `100000000`, `1000000000` |
| **Batches** | ~91 | `100`, `1000` |
| **Config File** | ~115 | See game configs below |
| **Memory Mode** | ~159 | `EFFICIENT`, `ACCURATE` |
| **Parallel** | ~122 | `true`, `false` |

### Game Configs

| Game | Config File |
|------|-------------|
| **SS03Game** | `SS03_Merged_Flattened_Output.json` |
| **DeepDive** | `SS02_Config_Table01_v1.json` |
| **DeepDive** | `DeepDive_config_test_4.json` |

---

## 🛠️ Troubleshooting

| Problem | Solution |
|---------|----------|
| "array index 4" error | `./build_ss03.sh` (rebuild) |
| Namespace conflict | `rm -rf build simulator && ./build_ss03.sh` |
| Wrong game loaded | Check which build script you ran |
| Config file not found | Update path in MonteCarlo_main.cpp |
| Memory error | Reduce simulations or use EFFICIENT mode |
| Red squiggles in VS Code | See `.vscode/README_INTELLISENSE.md` |

---

## 📊 Memory Guide

| Simulations | EFFICIENT | ACCURATE |
|-------------|-----------|----------|
| 10M | 100 MB | 770 MB |
| 100M | 100 MB | 7.7 GB |
| 1B | 100 MB | 77 GB ⚠️ |

**Rule:** Use EFFICIENT for 100M+ simulations

---

## 📂 Important Files

```
MonteCarlo_main.cpp    ← Edit simulation parameters
build_ss03.sh          ← Build for SS03Game
build_deepdive.sh      ← Build for DeepDive
simulator              ← Run this
WALKTHROUGH.md         ← Detailed guide
```

---

## 🚀 Typical Workflow

```bash
# 1. Build
./build_ss03.sh

# 2. (Optional) Edit parameters
code MonteCarlo_main.cpp

# 3. Rebuild if edited
./build_ss03.sh

# 4. Run
./build/simulator

# 5. Save results
./build/simulator > my_results.txt
```

---

## ⌨️ VS Code Shortcuts

| Action | Shortcut |
|--------|----------|
| Build | `Cmd+Shift+B` |
| Run/Debug | `F5` |
| Command Palette | `Cmd+Shift+P` |
| Terminal | ``Ctrl+` `` |

---

## 🎓 Remember

✅ **Game module = Build time decision**
- Can't switch by editing code
- Must rebuild with different script

✅ **Config file = Your responsibility**
- Must match game module
- Edit in MonteCarlo_main.cpp

✅ **Always rebuild after editing**
- Changes to .cpp files require rebuild
- Run appropriate build script

---

**Need more help?**
- Detailed walkthrough: `WALKTHROUGH.md`
- Build system: `BUILD.md`
- Full documentation: `README.md`
