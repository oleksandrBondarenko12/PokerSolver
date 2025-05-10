#include "gtest/gtest.h"
// Assuming test_scenario_loader.h is findable or its content is here
#include "test_scenario_loader.h" // Or where TestScenario related code is
#include "solver/PCfrSolver.h"
#include "compairer/Dic5Compairer.h"
#include "ranges/PrivateCardsManager.h"
#include "ranges/RiverRangeManager.h"
#include "GameTree.h"
#include "Deck.h"
#include <memory>
#include <filesystem> // For path joining if needed
#include <fstream>    // For load_json_file AND std::ofstream
#include <iostream>   // For std::cout
#include <iomanip>    // For std::setprecision
#include <sstream>    // For ostringstream
#include <string>     // For std::string manipulation

// Use aliases for convenience
using json = nlohmann::json;
namespace core = poker_solver::core;
namespace config = poker_solver::config;
namespace ranges = poker_solver::ranges;
namespace solver = poker_solver::solver;
namespace tree = poker_solver::tree;
namespace eval = poker_solver::eval;

// Helper to load a JSON file (e.g., for golden output)
json load_json_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        std::cerr << "Warning: Could not open JSON file for comparison: " << filepath << std::endl;
        return nullptr; // Return null json if file not found
    }
    json j;
    try {
        ifs >> j;
    } catch (const json::parse_error& e) {
        std::ostringstream oss;
        oss << "Warning: Failed to parse JSON file for comparison: " << filepath << " - " << e.what() << " at byte " << e.byte;
        std::cerr << oss.str() << std::endl;
        return nullptr;
    }
    return j;
}

// Basic comparison - can be enhanced later
bool compare_json_outputs(const json& actual, const json& expected, double tolerance = 1e-5) {
    if (actual.is_null() && expected.is_null()) return true;
    if (actual.is_null() || expected.is_null()) {
        std::cerr << "JSON comparison failed: one is null, the other is not." << std::endl;
        return false;
    }

    if (actual.type() != expected.type()) {
        std::cerr << "JSON type mismatch: actual=" << actual.type_name() << ", expected=" << expected.type_name() << std::endl;
        return false;
    }

    if (actual.is_object()) {
        if (actual.size() != expected.size()) {
             std::cerr << "JSON object size mismatch. Actual size: " << actual.size() << ", Expected size: " << expected.size() << std::endl;
             std::cerr << "Actual keys: "; for (auto& el : actual.items()) std::cerr << el.key() << ", "; std::cerr << std::endl;
             std::cerr << "Expected keys: "; for (auto& el : expected.items()) std::cerr << el.key() << ", "; std::cerr << std::endl;
             return false;
        }
        for (auto& [key, val_actual] : actual.items()) {
            if (!expected.contains(key)) {
                 std::cerr << "Expected JSON missing key: " << key << std::endl;
                 return false;
            }
            if (!compare_json_outputs(val_actual, expected[key], tolerance)) {
                std::cerr << "Difference found at key: " << key << std::endl;
                return false;
            }
        }
    } else if (actual.is_array()) {
        if (actual.size() != expected.size()) {
            std::cerr << "JSON array size mismatch. Actual size: " << actual.size() << ", Expected size: " << expected.size() << std::endl;
            return false;
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            if (!compare_json_outputs(actual[i], expected[i], tolerance)) {
                 std::cerr << "Difference found at array index: " << i << std::endl;
                return false;
            }
        }
    } else if (actual.is_number()) {
        if (!expected.is_number()) {
            std::cerr << "Numeric type mismatch: actual is number, expected is " << expected.type_name() << std::endl;
            return false;
        }
        if (std::abs(actual.get<double>() - expected.get<double>()) > tolerance) {
            std::cerr << std::fixed << std::setprecision(10) << "Numeric difference: actual=" << actual.get<double>()
                      << ", expected=" << expected.get<double>() << ", tolerance=" << tolerance << std::endl;
            return false;
        }
    } else { // boolean, string, null (null handled at start)
        if (actual != expected) {
             std::cerr << "Value difference: actual=" << actual << ", expected=" << expected << std::endl;
            return false;
        }
    }
    return true;
}


class PCfrSolverIntegrationTest : public ::testing::Test {
protected:
    core::Deck deck_;
    std::shared_ptr<eval::Dic5Compairer> compairer_;

    // This will be managed per test case by LoadAndSetupSolverForScenario
    std::unique_ptr<TestScenario> current_scenario_;


    void SetUp() override {
        try {
            // Path relative to build/test execution directory
            compairer_ = std::make_shared<eval::Dic5Compairer>("five_card_strength.txt");
        } catch (const std::exception& e) {
            FAIL() << "Fixture SetUp failed to load compairer: " << e.what();
        }
    }

    // Make LoadAndSetupScenario return the solver
    std::unique_ptr<solver::PCfrSolver> LoadAndSetupSolverForScenario(const std::string& scenario_filepath) {
        // Load the scenario structure using the helper
        // The TestScenario constructor will parse the JSON once.
        try {
            current_scenario_ = std::make_unique<TestScenario>(load_test_scenario(scenario_filepath, deck_));
        } catch (const std::exception& e) {
            ADD_FAILURE() << "Failed to load and parse scenario (TestScenario construction threw): "
                          << scenario_filepath << " - " << e.what();
            return nullptr; // Explicitly return nullptr after ADD_FAILURE
        }

        if (!current_scenario_) {
            ADD_FAILURE() << "Failed to load and parse scenario (current_scenario_ is null): " << scenario_filepath;
            return nullptr;
        }

        uint64_t initial_board_mask = core::Card::CardIntsToUint64(current_scenario_->initial_board_ints_for_pcm);

        std::shared_ptr<ranges::PrivateCardsManager> pcm;
        std::shared_ptr<ranges::RiverRangeManager> rrm;
        std::shared_ptr<tree::GameTree> game_tree;

        try {
            pcm = std::make_shared<ranges::PrivateCardsManager>(
                std::vector<std::vector<core::PrivateCards>>{current_scenario_->range_ip, current_scenario_->range_oop},
                initial_board_mask
            );
            rrm = std::make_shared<ranges::RiverRangeManager>(compairer_);
            game_tree = std::make_shared<tree::GameTree>(current_scenario_->game_rule);
        } catch (const std::exception& e) {
            ADD_FAILURE() << "Failed to create managers or game tree for scenario '"
                          << current_scenario_->test_case_name << "': " << e.what();
            return nullptr;
        }

        try {
            return std::make_unique<solver::PCfrSolver>(
                game_tree, pcm, rrm, current_scenario_->game_rule, current_scenario_->solver_config
            );
        } catch (const std::exception& e) {
            ADD_FAILURE() << "Failed to create PCfrSolver for scenario '"
                          << current_scenario_->test_case_name << "': " << e.what();
            return nullptr;
        }
    }
};

// Test using the "simple_flop_scenario.json"
TEST_F(PCfrSolverIntegrationTest, SimpleFlopTest) {
    std::string scenario_file = "test_data/simple_flop_scenario.json";
    std::unique_ptr<solver::PCfrSolver> solver = LoadAndSetupSolverForScenario(scenario_file);

    ASSERT_NE(solver, nullptr) << "Solver setup failed for scenario: " << scenario_file;
    ASSERT_NE(current_scenario_, nullptr) << "Current scenario is null after solver setup for: " << scenario_file;


    ASSERT_NO_THROW(solver->Train());

    json actual_output_json;
    ASSERT_NO_THROW(actual_output_json = solver->DumpStrategy(true, 3));

    // Output to terminal (as before, for immediate feedback)
    std::cout << "SimpleFlopTest Actual Output for " << current_scenario_->test_case_name << ":\n"
              << actual_output_json.dump(2) << std::endl;

    // --- Save to JSON file ---
    std::string output_filename = current_scenario_->test_case_name + "_actual_output.json";
    // Replace spaces or invalid characters in filename if necessary
    std::replace(output_filename.begin(), output_filename.end(), ' ', '_');
    // You might want to place it in a specific output directory, e.g., "test_outputs/"
    // For simplicity, saving in the current execution directory (usually build/tests/)
    std::string full_output_path = output_filename; // Can be prepended with a directory path

    std::ofstream out_file(full_output_path);
    if (out_file.is_open()) {
        out_file << actual_output_json.dump(2); // Use pretty print with indent 2
        out_file.close();
        std::cout << "Actual output saved to: " << full_output_path << std::endl;
    } else {
        std::cerr << "Warning: Could not open file to save actual output: " << full_output_path << std::endl;
        // Decide if this should be a test failure
        // ADD_FAILURE() << "Could not save actual output to file: " << full_output_path;
    }
    // --- End of save to JSON file ---


    if (!current_scenario_->expected_output_file.empty()) {
        std::string golden_file_path = "test_data/" + current_scenario_->expected_output_file;
        json expected_output = load_json_file(golden_file_path);
        if (!expected_output.is_null()) {
             EXPECT_TRUE(compare_json_outputs(actual_output_json, expected_output, 1e-4))
                 << "Output for " << current_scenario_->test_case_name
                 << " does not match golden file: " << golden_file_path;
        } else {
             std::cout << "Golden file " << golden_file_path << " not found or empty. Manual inspection required for "
                       << current_scenario_->test_case_name << std::endl;
        }
    } else {
        std::cout << "No golden file specified for " << current_scenario_->test_case_name << ". Manual inspection required." << std::endl;
    }
}