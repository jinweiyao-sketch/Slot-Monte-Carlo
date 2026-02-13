cmake_minimum_required(VERSION 3.15)
project(MonteCarloSimulator CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Option to select which game module to use
set(GAME_MODULE "SS03Game" CACHE STRING "Game module to use (DeepDive or SS03Game)")
set_property(CACHE GAME_MODULE PROPERTY STRINGS "DeepDive" "SS03Game")

# Display selected game module
message(STATUS "Building with game module: ${GAME_MODULE}")

# Set the game-specific source and header files
if(GAME_MODULE STREQUAL "DeepDive")
    set(GAME_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/DeepDive.cpp")
    set(GAME_HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/DeepDive.h")
    add_compile_definitions(USE_DEEPDIVE)
elseif(GAME_MODULE STREQUAL "SS03Game")
    set(GAME_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/SS03Game.cpp")
    set(GAME_HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/SS03Game.h")
    add_compile_definitions(USE_SS03GAME)
else()
    message(FATAL_ERROR "Invalid GAME_MODULE: ${GAME_MODULE}. Must be 'DeepDive' or 'SS03Game'")
endif()

# Common source files
set(COMMON_SOURCES
    MonteCarlo_main.cpp
    MonteCarloSimulator.cpp
    Statistics.cpp
)

# Create the executable
add_executable(simulator
    ${COMMON_SOURCES}
    ${GAME_SOURCE_FILE}
)

# Find and link OpenMP
# On macOS, help CMake find Homebrew-installed libomp
if(APPLE)
    execute_process(COMMAND brew --prefix libomp
                    OUTPUT_VARIABLE LIBOMP_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(LIBOMP_PREFIX)
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_C_LIB_NAMES "omp")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
        include_directories(${LIBOMP_PREFIX}/include)
        link_directories(${LIBOMP_PREFIX}/lib)
    endif()
endif()

find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    target_link_libraries(simulator PUBLIC OpenMP::OpenMP_CXX)
endif()

# Print build information
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Game source file: ${GAME_SOURCE_FILE}")
message(STATUS "Game header file: ${GAME_HEADER_FILE}")
