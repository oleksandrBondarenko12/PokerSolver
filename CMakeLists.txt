# Require CMake 3.14+ for FetchContent_MakeAvailable
cmake_minimum_required(VERSION 3.14)

# Define the project name (can remain the same)
project(poker_solver_ui LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt Configuration ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
# --- End Qt Configuration ---

# --- Core Poker Solver Library ---
# Define a static library for the core, non-UI logic.
# Add all necessary source files for the core logic here.
# ASSUMPTION: Your source files are in src/* subdirectories mirroring include/*
add_library(PokerSolverCore STATIC
    src/Card.cpp
    src/Deck.cpp # Add if needed by Dic5Compairer tests indirectly
    src/compairer/Dic5Compairer.cpp
    src/tools/lookup8.cpp # Assuming this exists
    # Add other necessary core source files (e.g., Library.cpp, nodes if needed by compairer)
    # src/Library.cpp # If Library.h has corresponding source
    # ... add paths to other core .cpp files ...
)

# Make the 'include' directory available for the core library and anything linking to it
target_include_directories(PokerSolverCore PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
# --- End Core Poker Solver Library ---


# --- Main UI Executable ---
# Define the UI executable
add_executable(poker_solver_ui
    main.cpp
    MainWindow.cpp
    RangeSelector.cpp
    StrategyExplorer.cpp
    resources.qrc
)

# Link the UI executable against the core library and Qt
target_link_libraries(poker_solver_ui PRIVATE
    PokerSolverCore # Link against our core logic library
    Qt6::Widgets
)
# --- End Main UI Executable ---


# === Testing Setup ===
# Only proceed with test setup if BUILD_TESTING is ON (default is ON)
# You can disable tests by configuring CMake with -DBUILD_TESTING=OFF
option(BUILD_TESTING "Build the tests" ON)

if(BUILD_TESTING)
    # Enable CTest
    enable_testing()

    # --- Google Test Integration using FetchContent ---
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        # CORRECT TAG FORMAT
        GIT_TAG    v1.14.0
      )
    # Make GTest available (downloads/builds if necessary)
    FetchContent_MakeAvailable(googletest)
    # --- End Google Test Integration ---

    # --- Test Executable Definition ---
    # Define an executable specifically for testing Dic5Compairer
    # ASSUMPTION: Your test code is in tests/test_dic5_compairer.cpp
    add_executable(dic5_compairer_test
        tests/test_dic5_compairer.cpp
    )

    # Link the test executable against:
    # 1. The core library (which contains Dic5Compairer and its dependencies)
    # 2. Google Test libraries
    target_link_libraries(dic5_compairer_test PRIVATE
        PokerSolverCore   # Provides Dic5Compairer, Card, lookup8 etc.
        GTest::gtest
        GTest::gtest_main # Provides main() for the test runner
    )

    # --- Data File Handling for Tests ---
    # Define source and destination paths
    set(DATA_FILE_SOURCE "${PROJECT_SOURCE_DIR}/resources/five_card_strength.txt")
    set(DATA_FILE_DEST "${CMAKE_CURRENT_BINARY_DIR}/five_card_strength.txt") # Destination is the build directory

    # Add a check during configuration to ensure the source file exists
    if(NOT EXISTS "${DATA_FILE_SOURCE}")
        message(FATAL_ERROR "Test data file not found at expected location: ${DATA_FILE_SOURCE}")
    endif()

    # Custom command to copy the file after the test target is built
    add_custom_command(
        TARGET dic5_compairer_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DATA_FILE_SOURCE}"  # Use the variable which should hold the full path
            "${DATA_FILE_DEST}"
        COMMENT "Copying five_card_strength.txt for tests"
        VERBATIM # Helps ensure arguments are passed exactly as written
    )
    # --- End Data File Handling ---


    # --- Test Discovery ---
    # Automatically discover tests defined using Google Test macros
    include(GoogleTest)
    gtest_discover_tests(dic5_compairer_test)
    # --- End Test Discovery ---

endif()
# === End Testing Setup ===
