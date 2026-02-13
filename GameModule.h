#ifndef GAME_MODULE_H
#define GAME_MODULE_H

// This header automatically includes the correct game module
// based on the CMake configuration

#if defined(USE_DEEPDIVE)
    #include "DeepDive.h"
#elif defined(USE_SS03GAME)
    #include "SS03Game.h"
#else
    #error "No game module defined! Set GAME_MODULE in CMake"
#endif

#endif // GAME_MODULE_H
