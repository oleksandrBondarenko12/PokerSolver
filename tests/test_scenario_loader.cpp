#include "test_scenario_loader.h" // Includes declarations and inline definitions
#include "json.hpp"
#include "Card.h"
#include "tools/PrivateRangeConverter.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <optional>

// Use aliases
using json = nlohmann::json;
namespace core = poker_solver::core;
namespace config = poker_solver::config;
namespace ranges = poker_solver::ranges;
namespace solver = poker_solver::solver;

// *** REMOVE THE DEFINITIONS of get_json_value_or_default, parse_street_setting, ***
// *** and create_rule_from_json FROM THIS .CPP FILE. They are now in the header. ***

TestScenario::TestScenario(const json& j_scenario, const core::Deck& deck) :
    solver_config(), // Default construct
    game_rule(create_rule_from_json(j_scenario.at("game_rule"), deck)) // Calls inline from header
{
    try {
        test_case_name = j_scenario.at("test_case_name").get<std::string>();
        description = get_json_value_or_default(j_scenario, "description", ""); // Calls inline from header

        if (j_scenario.contains("solver_config")) {
            const auto& j_sc = j_scenario.at("solver_config");
            solver_config.iteration_limit = get_json_value_or_default(j_sc, "iterations", 1);
            solver_config.num_threads = get_json_value_or_default(j_sc, "threads", 100);
        }

        // Populate initial_board_ints_for_pcm from the game_rule member
        // which was constructed using initial_board from JSON via create_rule_from_json
        this->initial_board_ints_for_pcm = this->game_rule.GetInitialBoardCardsInt();


        const auto& j_ranges = j_scenario.at("player_ranges");
        std::string range_ip_str = j_ranges.at("ip").get<std::string>();
        std::string range_oop_str = j_ranges.at("oop").get<std::string>();

        range_ip = ranges::PrivateRangeConverter::StringToPrivateCards(range_ip_str, this->initial_board_ints_for_pcm);
        range_oop = ranges::PrivateRangeConverter::StringToPrivateCards(range_oop_str, this->initial_board_ints_for_pcm);

        expected_output_file = get_json_value_or_default(j_scenario, "expected_output_file", "");

    } catch (const json::exception& e) {
        std::ostringstream oss;
        oss << "JSON parsing/access error for TestScenario '" << (j_scenario.contains("test_case_name") ? j_scenario.at("test_case_name").get<std::string>() : "UNKNOWN")
            << "' (ID: " << e.id << "): " << e.what();
        throw std::runtime_error(oss.str());
    } catch (const std::exception& e) {
         std::ostringstream oss;
        oss << "Error during TestScenario '" << (j_scenario.contains("test_case_name") ? j_scenario.at("test_case_name").get<std::string>() : "UNKNOWN")
            << "' construction: " << e.what();
        throw std::runtime_error(oss.str());
    }
}

TestScenario load_test_scenario(const std::string& scenario_filepath, const core::Deck& deck) {
    std::ifstream ifs(scenario_filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open scenario file: " + scenario_filepath);
    }
    json j_scenario;
    try {
        ifs >> j_scenario;
    } catch (const json::parse_error& e) {
        std::ostringstream oss;
        oss << "Failed to parse scenario JSON: " << e.what()
            << " in file: " << scenario_filepath << " at byte " << e.byte;
        throw std::runtime_error(oss.str());
    }
    return TestScenario(j_scenario, deck);
}