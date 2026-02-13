# Setup Summary - CMake Build System

## Problem Fixed

**Issue**: VS Code tasks.json was compiling `DeepDive.cpp` while headers included `SS03Game.h`, causing namespace conflicts and "array index 4 out of range" errors.

**Root Cause**: Manual include switching was error-prone and inconsistent across files.

## Solution Implemented

Created a professional CMake-based build system with automatic game module selection.

---

## New Project Structure

### Core Build Files

```
SlotMC/
├── CMakeLists.txt          # Main CMake configuration
├── GameModule.h            # Smart wrapper header (auto-includes correct game)
├── build_ss03.sh           # Quick build script for SS03Game
├── build_deepdive.sh       # Quick build script for DeepDive
├── BUILD.md                # Detailed build documentation
└── .vscode/
    ├── tasks.json          # Updated with CMake tasks
    └── launch.json         # Updated to use new build tasks
```

### How It Works

1. **GameModule.h** - New wrapper header:
   ```cpp
   #if defined(USE_DEEPDIVE)
       #include "DeepDive.h"
   #elif defined(USE_SS03GAME)
       #include "SS03Game.h"
   #endif
   ```

2. **All source files** now include `GameModule.h` instead of game-specific headers:
   - `MonteCarloSimulator.h`
   - `MonteCarloSimulator.cpp`
   - `MonteCarlo_main.cpp`

3. **CMakeLists.txt** controls which game is compiled:
   - Sets preprocessor definitions (`USE_DEEPDIVE` or `USE_SS03GAME`)
   - Compiles only the selected game module's .cpp file
   - Prevents namespace conflicts

---

## Usage

### Method 1: Quick Build Scripts (Easiest)

**For SS03Game:**
```bash
./build_ss03.sh
./build/simulator
```

**For DeepDive:**
```bash
./build_deepdive.sh
./build/simulator
```

### Method 2: VS Code Tasks

1. Press `Cmd+Shift+P`
2. Select "Tasks: Run Build Task"
3. Choose:
   - `Build Project Simulator (SS03Game)` ← **Default**
   - `Build Project Simulator (DeepDive)`

The debugger will automatically use the selected module.

### Method 3: Manual CMake

**SS03Game:**
```bash
cmake -B build -DGAME_MODULE=SS03Game
cmake --build build
```

**DeepDive:**
```bash
cmake -B build -DGAME_MODULE=DeepDive
cmake --build build
```

---

## Benefits

✅ **No manual include editing** - Change one CMake option instead of 3+ files
✅ **Prevents conflicts** - Only one game module compiled at a time
✅ **VS Code integration** - Build directly from IDE
✅ **Type safety** - Compile-time errors if game not selected
✅ **Scalable** - Easy to add new game modules
✅ **Professional** - Industry-standard CMake build system

---

## Adding a New Game Module

1. Create `NewGame.h` and `NewGame.cpp` with the same interface:
   - Must define `namespace Game`
   - Must implement all required functions

2. Update `CMakeLists.txt`:
   ```cmake
   elseif(GAME_MODULE STREQUAL "NewGame")
       set(GAME_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/NewGame.cpp")
       add_compile_definitions(USE_NEWGAME)
   ```

3. Update `GameModule.h`:
   ```cpp
   #elif defined(USE_NEWGAME)
       #include "NewGame.h"
   ```

4. Create `build_newgame.sh` for convenience

5. Add VS Code task in `tasks.json`

---

## Files Modified

### Created
- `CMakeLists.txt`
- `GameModule.h`
- `build_ss03.sh`
- `build_deepdive.sh`
- `BUILD.md`
- `SETUP_SUMMARY.md`
- `.gitignore`

### Modified
- `MonteCarloSimulator.h` - Now includes `GameModule.h`
- `MonteCarloSimulator.cpp` - Removed game-specific includes
- `MonteCarlo_main.cpp` - Removed game-specific includes
- `.vscode/tasks.json` - CMake-based build tasks
- `.vscode/launch.json` - Updated pre-launch task

---

## Troubleshooting

**Q: Build fails with "No game module defined"**
A: Make sure you specify `-DGAME_MODULE=SS03Game` or use a build script

**Q: Still getting "array index 4" error**
A: Clean rebuild: `rm -rf build simulator && ./build_ss03.sh`

**Q: VS Code debugger fails**
A: Make sure you ran the correct build task first (default is SS03Game)

**Q: Want to switch games**
A: Just run a different build script or task - no need to clean

---

## Current Configuration

**Default Game**: SS03Game
**Build Directory**: `build/`
**Output Executable**: `simulator` (copied to root)
**Compiler**: g++-15 from Homebrew
**OpenMP**: Enabled
**Build Type**: Release (for scripts), Debug (for VS Code)

---

## Next Steps

Your build system is now set up! To use it:

1. Run `./build_ss03.sh` to build for SS03Game
2. Run `./build/simulator` to execute
3. When you need DeepDive, just run `./build_deepdive.sh`

**No more manual include editing required!** 🎉
