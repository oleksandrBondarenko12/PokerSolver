# CMakeLists.txt (Root or where tests are defined)
cmake_minimum_required(VERSION 3.10) # Or your project's minimum
project(PokerSolver LANGUAGES CXX)

# --- Set C++ Standard ---
set(CMAKE_CXX_STANDARD 17) # Use C++17 (required for std::optional, std::filesystem)
set(CMAKE_CXX_STANDARD_REQUIRED True) # Error if C++17 is not supported
set(CMAKE_CXX_EXTENSIONS OFF) # Prefer standard features over extensions
# ------------------------

# --- Add AddressSanitizer Flags for Debug Builds ---
# Check if build type is Debug (or default/empty which often implies debug flags)
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Enabling AddressSanitizer for Debug build.")
  # Add flags for compiler
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address")
  # Add flags for linker (cover executable, shared, module libraries)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=address")
else()
   message(STATUS "AddressSanitizer disabled for ${CMAKE_BUILD_TYPE} build.")
   # Optionally add optimization flags for Release builds
   # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()
# --------------------------------------------------


find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)

# +++ Additions for Qt UI +++
# Find Qt6 package and its components.
# Adjust components if you use more Qt modules (e.g., Network, Svg).
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# Enable automatic moc, uic, and rcc handling.
# CMAKE_AUTOMOC: Enables automatic meta-object code generation.
# CMAKE_AUTOUIC: Enables automatic user interface code generation from .ui files (if any).
# CMAKE_AUTORCC: Enables automatic resource code generation from .qrc files.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Ensure that CMake finds the Qt6 tools (moc, uic, rcc).
# This is often handled automatically if Qt6 is in your PATH or CMAKE_PREFIX_PATH.
# If you have issues, you might need to set CMAKE_PREFIX_PATH to your Qt6 installation directory.
# Example: set(CMAKE_PREFIX_PATH "/path/to/your/Qt/6.x.y/arch")
# +++++++++++++++++++++++++++

# --- Define Poker Solver Core Library ---
add_library(PokerSolverCore
    # Add ALL your core source files here
    src/Card.cpp
    src/Deck.cpp
    src/ranges/PrivateCards.cpp
    src/tools/StreetSetting.cpp
    src/Library.cpp
    src/tools/lookup8.cpp
    src/compairer/Dic5Compairer.cpp
    src/ranges/RiverCombs.cpp
    src/tools/PrivateRangeConverter.cpp
    src/ranges/RiverRangeManager.cpp
    src/tools/GameTreeBuildingSettings.cpp
    src/tools/Rule.cpp
    src/ranges/PrivateCardsManager.cpp
    src/nodes/GameActions.cpp
    src/nodes/GameTreeNode.cpp
    src/nodes/ActionNode.cpp
    src/nodes/ChanceNode.cpp
    src/nodes/ShowdownNode.cpp
    src/nodes/TerminalNode.cpp
    src/trainable/DiscountedCfrTrainable.cpp
    # src/trainable/CFRPlus.cpp # Assuming you might add this back or have it
    # src/trainable/Trainable.cpp # If it has a .cpp, add it. If header-only, no need.
    src/GameTree.cpp
    src/solver/Solver.cpp
    src/solver/PCfrSolver.cpp
    # src/builder/GameTreeBuilder.cpp # Add if actually used and compiled
    # src/kuhn/kuhn_poker_setup.cpp # Assuming you have this for Kuhn tests
)

# Specify include directories for the PokerSolverCore library
# PUBLIC means these includes are also available to targets linking PokerSolverCore
target_include_directories(PokerSolverCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann # Add path to nlohmann/json if it's here
)
# For std::filesystem
target_link_libraries(PokerSolverCore PRIVATE Threads::Threads) # Threads might not be needed for filesystem

# Filesystem linking (keep as is)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(PokerSolverCore PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7) # Or appropriate Clang version
    target_link_libraries(PokerSolverCore PRIVATE c++experimental)
endif()

# --- OpenMP Section ---
# Use the flags found by find_package(OpenMP) - should work with Homebrew's Clang
target_compile_options(PokerSolverCore PRIVATE ${OpenMP_CXX_FLAGS})

# Link the library using the imported target found by find_package
target_link_libraries(PokerSolverCore PUBLIC OpenMP::OpenMP_CXX)
# --- End OpenMP Section ---

# +++ Define the UI Executable +++
# List your Qt resource file(s) here.
# The README mentions "resources.qrc" in the root.
# If dark.qss is not included via resources.qrc but compiled directly (unlikely for Qt),
# you might need to add it here, but typically it's part of the .qrc.
set(UI_RESOURCE_FILES
    resources.qrc  # Assuming this is in the root directory of your project
)

add_executable(poker_solver_ui
    main.cpp
    MainWindow.cpp
    MainWindow.h # Add header files that need MOC processing if not automatically detected
    RangeSelector.cpp
    RangeSelector.h # Add header files that need MOC processing
    StrategyExplorer.cpp
    StrategyExplorer.h # Add header files that need MOC processing
    ${UI_RESOURCE_FILES} # Include the .qrc file
)

# Link the UI executable against Qt6 and your PokerSolverCore library
target_link_libraries(poker_solver_ui PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    PokerSolverCore # Link against your core logic
)
# ++++++++++++++++++++++++++++++


# --- Google Test Integration ---
set(INSTALL_GTEST OFF CACHE BOOL "Disable installing gtest")
add_subdirectory(third_party/googletest) # Make sure this path is correct
enable_testing()

# --- Define Your Test Executable ---
add_executable(poker_solver_tests
    tests/card_test.cpp
    tests/deck_test.cpp
    tests/private_cards_test.cpp
    tests/dic5_compairer_test.cpp
    tests/river_range_manager_test.cpp
    tests/private_range_converter_test.cpp
    tests/game_tree_building_settings_test.cpp
    tests/rule_test.cpp
    tests/private_card_manager_test.cpp
    tests/utils_test.cpp
    tests/action_node_test.cpp
    tests/chance_node_test.cpp
    tests/showdown_node_test.cpp
    tests/terminal_node_test.cpp
    tests/discounted_cfr_trainable_test.cpp
    tests/game_tree_test.cpp
    tests/test_scenario_loader.cpp
    tests/pcfr_solver_integration_test.cpp
    # Add your test scenario loader .cpp file if it's separate
    # tests/test_scenario_loader.cpp
)

# Link your tests against Google Test AND your core library
target_link_libraries(poker_solver_tests PRIVATE
    gtest_main
    PokerSolverCore # This will inherit the OpenMP flags and linking from PokerSolverCore
)
# For std::filesystem in tests if needed (e.g. in test_scenario_loader.cpp)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(poker_solver_tests PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
    target_link_libraries(poker_solver_tests PRIVATE c++experimental)
endif()


# --- Copy Test Data ---
# Copy the hand strength dictionary
set(TEST_DICT_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_data/five_card_strength.txt)
set(TEST_DICT_DEST_PATH ${CMAKE_CURRENT_BINARY_DIR}/five_card_strength.txt)
add_custom_command(
    TARGET poker_solver_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TEST_DICT_SOURCE_PATH}"
            "${TEST_DICT_DEST_PATH}"
    COMMENT "Copying dictionary for tests..."
)

# --- NEW: Copy JSON Scenario Files ---
# Define the source directory for your JSON test scenarios
set(TEST_JSON_SCENARIOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_data)

# Define the destination directory in the build folder
set(TEST_JSON_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_data)

# Create the destination directory if it doesn't exist
add_custom_command(
    TARGET poker_solver_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_JSON_DEST_DIR}"
    COMMENT "Creating test_data directory in build folder..."
)

# Find all .json files in the source directory and copy them
# This uses a glob, which will find files at configure time.
# If you add new JSON files, you'll need to re-run CMake.
file(GLOB_RECURSE JSON_SCENARIO_FILES RELATIVE "${TEST_JSON_SCENARIOS_DIR}" "${TEST_JSON_SCENARIOS_DIR}/*.json")

foreach(JSON_FILE ${JSON_SCENARIO_FILES})
    set(JSON_SOURCE_PATH "${TEST_JSON_SCENARIOS_DIR}/${JSON_FILE}")
    set(JSON_DEST_PATH "${TEST_JSON_DEST_DIR}/${JSON_FILE}")
    add_custom_command(
        TARGET poker_solver_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${JSON_SOURCE_PATH}"
                "${JSON_DEST_PATH}"
        COMMENT "Copying test scenario: ${JSON_FILE}..."
    )
endforeach()
# ----------------------------------

include(GoogleTest)
gtest_discover_tests(poker_solver_tests)
