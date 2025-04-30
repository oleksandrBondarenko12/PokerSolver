# CMakeLists.txt (Root or where tests are defined)
cmake_minimum_required(VERSION 3.10) # Or your project's minimum
project(PokerSolver LANGUAGES CXX)

# --- Set C++ Standard ---
set(CMAKE_CXX_STANDARD 17) # Use C++17 (required for std::optional)
set(CMAKE_CXX_STANDARD_REQUIRED True) # Error if C++17 is not supported
set(CMAKE_CXX_EXTENSIONS OFF) # Prefer standard features over extensions
# ------------------------


find_package(Threads REQUIRED)
find_package(OpenMP)

# --- Define Poker Solver Core Library ---
add_library(PokerSolverCore
    # Add ALL your core source files here
    # IMPORTANT: Verify these paths match your project structure exactly!
    src/Card.cpp           # Example path
    src/Deck.cpp           # Example path
    src/ranges/PrivateCards.cpp  # Example path
    src/tools/StreetSetting.cpp # Example path
    src/Library.cpp         # Example path
    src/tools/lookup8.cpp
    src/compairer/Dic5Compairer.cpp
    src/ranges/RiverCombs.cpp # <<< --- ADD THIS LINE (Verify path!)
    src/tools/PrivateRangeConverter.cpp # <<< ADD THIS LINE
    src/ranges/RiverRangeManager.cpp # <<< --- ADD THIS LINE (Verify path!)
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
    src/GameTree.cpp
    src/solver/Solver.cpp
    src/solver/best_response.cpp
    
    # Example path
    # ... add other .cpp files for your core logic as you create them ...
)

# Specify include directories for the PokerSolverCore library
# PUBLIC means these includes are also available to targets linking PokerSolverCore
target_include_directories(PokerSolverCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include # Adjust path if your include dir is elsewhere
)

# --- Link OpenMP if found ---
# Disable OpenMP for Apple Clang/macOS by default
# You can re-enable this block if you install OpenMP and configure Clang properly.
# if(OpenMP_CXX_FOUND)
#     target_link_libraries(PokerSolverCore PUBLIC OpenMP::OpenMP_CXX)
#     message(STATUS "OpenMP found and enabled via CMake module.")
# else()
#    find_library(OpenMP_LIBRARY NAMES omp libomp libgomp HINTS /opt/homebrew/opt/libomp/lib)
#    if(OpenMP_LIBRARY)
#        target_link_libraries(PokerSolverCore PUBLIC ${OpenMP_LIBRARY})
#        target_compile_options(PokerSolverCore PUBLIC -Xpreprocessor -fopenmp)
#        target_link_options(PokerSolverCore PUBLIC -fopenmp)
#        message(STATUS "OpenMP found manually at ${OpenMP_LIBRARY}")
#    else()
#        message(WARNING "OpenMP not found. Parallelism will be disabled.")
#    endif()
# endif()

# --- Link OpenMP if found ---
# if(OpenMP_CXX_FOUND)
#     target_link_libraries(PokerSolverCore PUBLIC OpenMP::OpenMP_CXX)
#     message(STATUS "OpenMP found and enabled via CMake module.")
# else()
#     find_library(OpenMP_LIBRARY NAMES omp libomp libgomp HINTS /opt/homebrew/opt/libomp/lib)
#     find_path(OpenMP_INCLUDE_DIR NAMES omp.h HINTS /opt/homebrew/opt/libomp/include)
#     if(OpenMP_LIBRARY AND OpenMP_INCLUDE_DIR)
#         target_link_libraries(PokerSolverCore PUBLIC ${OpenMP_LIBRARY})
#         target_include_directories(PokerSolverCore PUBLIC ${OpenMP_INCLUDE_DIR})
#         target_compile_options(PokerSolverCore PUBLIC -Xpreprocessor -fopenmp)
#         target_link_options(PokerSolverCore PUBLIC -fopenmp)
#         message(STATUS "OpenMP found manually at ${OpenMP_LIBRARY} and ${OpenMP_INCLUDE_DIR}")
#     else()
#         message(WARNING "OpenMP not found (Library: ${OpenMP_LIBRARY}, Include: ${OpenMP_INCLUDE_DIR}). Parallelism will be disabled.")
#     endif()
# endif()

# --- Add your main application executable target here (if you have one) ---
# Example:
# add_executable(MyPokerApp src/main.cpp)
# target_link_libraries(MyPokerApp PRIVATE PokerSolverCore)
# ---

# --- Google Test Integration ---
# Prevent gtest from installing itself globally with your project install
set(INSTALL_GTEST OFF CACHE BOOL "Disable installing gtest")

# Add the googletest directory (adjust path if needed)
# This assumes googletest is in third_party/googletest relative to this CMakeLists.txt
add_subdirectory(third_party/googletest)

# Enable CMake testing support
enable_testing()

# --- Define Your Test Executable ---
add_executable(poker_solver_tests
    # IMPORTANT: Verify these paths match your project structure exactly!
    tests/card_test.cpp
    tests/deck_test.cpp
    tests/private_cards_test.cpp
    tests/dic5_compairer_test.cpp
    tests/river_range_manager_test.cpp
    tests/private_range_converter_test.cpp # <<< ADD THIS LINE
    tests/game_tree_building_settings_test.cpp # <<< ADD THIS LINE
    tests/rule_test.cpp
    tests/private_card_manager_test.cpp
    tests/utils_test.cpp # <<< ADD THIS LINE
    tests/action_node_test.cpp
    tests/chance_node_test.cpp # <<< ADD THIS LINE
    tests/showdown_node_test.cpp
    tests/terminal_node_test.cpp
    tests/discounted_cfr_trainable_test.cpp
    tests/game_tree_test.cpp # <<< ADD THIS LINE
    tests/best_response_test.cpp # <<< ADD THIS LINE


    # <<< ADD THIS LINE

    # Add other test files here...
)

# Include directories for tests are usually inherited from linked libraries.
# If tests need specific additional include paths, add them here:
# target_include_directories(poker_solver_tests PRIVATE
#     # Example: ${CMAKE_CURRENT_SOURCE_DIR}/tests/include
# )

# Link your tests against Google Test AND your core library
target_link_libraries(poker_solver_tests PRIVATE
    gtest_main       # Google Test library + main()
    PokerSolverCore  # <<< --- ADDED THIS LINK! ---
)


# --- Copy Test Data ---
# Define the path to the dictionary file relative to the source directory
set(TEST_DICT_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_data/five_card_strength.txt)
# Define the destination path within the build directory (where the executable runs)
set(TEST_DICT_DEST_PATH ${CMAKE_CURRENT_BINARY_DIR}/five_card_strength.txt)

# Add a custom command to copy the file *after* the test executable is built
# This ensures the file is available when the test runs.
add_custom_command(
    TARGET poker_solver_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TEST_DICT_SOURCE_PATH}"
            "${TEST_DICT_DEST_PATH}"
    COMMENT "Copying dictionary for tests..."
)
# ----------------------

# Automatically discover tests in your executable (CMake 3.10+)
include(GoogleTest)
gtest_discover_tests(poker_solver_tests)
