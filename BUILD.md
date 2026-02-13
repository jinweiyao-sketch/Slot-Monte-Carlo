# Build Instructions

This project uses CMake for building with support for multiple game modules.

## Quick Start

### Option 1: Using Build Scripts (Recommended)

**Build for SS03Game:**
```bash
./build_ss03.sh
```

**Build for DeepDive:**
```bash
./build_deepdive.sh
```

Then run:
```bash
./build/simulator
```

### Option 2: Using CMake Directly

**Build for SS03Game:**
```bash
cmake -B build -DGAME_MODULE=SS03Game -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15
cmake --build build -j 10
```

**Build for DeepDive:**
```bash
cmake -B build -DGAME_MODULE=DeepDive -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-15
cmake --build build -j 10
```

### Option 3: Using VS Code

1. Open Command Palette (`Cmd+Shift+P`)
2. Select **Tasks: Run Build Task**
3. Choose either:
   - `Build Project Simulator (SS03Game)` - Default
   - `Build Project Simulator (DeepDive)`

The built executable will be at `build/simulator`

## Build System Architecture

### Game Module Selection

The project supports multiple game modules that share the same Monte Carlo simulator core:
- **SS03Game** - SS03 game mechanics
- **DeepDive** - DeepDive game mechanics

The game module is selected at compile-time using CMake's `GAME_MODULE` option.

### How It Works

1. **CMakeLists.txt** - Defines build options and selects game module
2. **GameModule.h** - Wrapper header that includes the correct game based on preprocessor definitions
3. All source files include `GameModule.h` instead of game-specific headers

### Adding a New Game Module

1. Create `NewGame.h` and `NewGame.cpp` following the same interface as `DeepDive.h`
2. Add to `CMakeLists.txt`:
   ```cmake
   elseif(GAME_MODULE STREQUAL "NewGame")
       set(GAME_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/NewGame.cpp")
       set(GAME_HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/NewGame.h")
       add_compile_definitions(USE_NEWGAME)
   ```
3. Add to `GameModule.h`:
   ```cpp
   #elif defined(USE_NEWGAME)
       #include "NewGame.h"
   ```
4. Create `build_newgame.sh` for convenience

## Clean Build

To clean all build artifacts:
```bash
rm -rf build simulator
```

Or in VS Code:
- Select **Tasks: Run Task** → **Clean Build**

## Troubleshooting

**Problem**: `cmake: command not found`
```bash
brew install cmake
```

**Problem**: Build fails with OpenMP errors
- Make sure you're using g++-15 from Homebrew
- Install with: `brew install gcc@15`

**Problem**: Old executable still running
- Clean build: `rm -rf build simulator`
- Rebuild from scratch

**Problem**: Namespace conflicts
- Make sure you only build with ONE game module at a time
- The wrapper header ensures only one game is included
