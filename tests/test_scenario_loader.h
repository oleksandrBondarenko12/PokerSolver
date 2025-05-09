#ifndef TEST_SCENARIO_LOADER_H
#define TEST_SCENARIO_LOADER_H

#include "tools/Rule.h"
#include "solver/PCfrSolver.h"
#include "ranges/PrivateCards.h"
#include "Deck.h"
#include "json.hpp"
#include "tools/StreetSetting.h"
#include "Card.h"
#include "tools/GameTreeBuildingSettings.h"
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <typeinfo>
#include <optional> // For Card::StringToInt

// Use aliases
using json = nlohmann::json;
namespace core = poker_solver::core;
namespace config = poker_solver::config;
namespace ranges = poker_solver::ranges;
namespace solver = poker_solver::solver;

// --- Inline Helper Function Definitions ---
template <typename T>
inline T get_json_value_or_default(const json& j, const std::string& key, const T& default_value) {
    if (j.contains(key)) {
        try {
            const auto& val_json = j.at(key);
            if ((std::is_integral_v<T> && val_json.is_number_integer()) ||
                (std::is_floating_point_v<T> && val_json.is_number_float()) ||
                (std::is_same_v<T, bool> && val_json.is_boolean()) ||
                (std::is_same_v<T, std::string> && val_json.is_string())) {
                return val_json.get<T>();
            } else if (val_json.is_number() && (std::is_integral_v<T> || std::is_floating_point_v<T>)) {
                return val_json.get<T>();
            }
            std::cerr << "Warning: JSON type mismatch for key '" << key
                      << "'. Expected C++ type '" << typeid(T).name()
                      << "', got JSON type '" << val_json.type_name()
                      << "'. Using default value (" << default_value << ")." << std::endl;
            return default_value;
        } catch (const json::type_error& e) {
            std::cerr << "Warning: JSON type error for key '" << key << "': " << e.what()
                      << ". Using default value (" << default_value << ")." << std::endl;
            return default_value;
        }  catch (const json::out_of_range& e) {
            std::cerr << "Warning: JSON key '" << key << "' not found or of unexpected type in object: " << e.what()
                      << ". Using default value (" << default_value << ")." << std::endl;
            return default_value;
        }
    }
    return default_value;
}

inline std::string get_json_value_or_default(const json& j, const std::string& key, const char* default_value) {
    if (j.contains(key) && j.at(key).is_string()) {
        return j.at(key).get<std::string>();
    }
    return std::string(default_value);
}

inline config::StreetSetting parse_street_setting(const json& j_ss) {
    std::vector<double> bets, raises, donks;
    bool allow_all_in = false;

    if (j_ss.contains("bet_sizes_percent") && j_ss.at("bet_sizes_percent").is_array()) {
        bets = j_ss.at("bet_sizes_percent").get<std::vector<double>>();
    }
    if (j_ss.contains("raise_sizes_percent") && j_ss.at("raise_sizes_percent").is_array()) {
        raises = j_ss.at("raise_sizes_percent").get<std::vector<double>>();
    }
    if (j_ss.contains("donk_sizes_percent") && j_ss.at("donk_sizes_percent").is_array()) {
        donks = j_ss.at("donk_sizes_percent").get<std::vector<double>>();
    }
    allow_all_in = get_json_value_or_default(j_ss, "allow_all_in", false);

    return config::StreetSetting(bets, raises, donks, allow_all_in);
}

// Helper function to create Rule from JSON (also inline in header)
inline config::Rule create_rule_from_json(const json& j_rule, const core::Deck& deck) {
    std::string starting_round_str = j_rule.at("starting_round").get<std::string>();
    core::GameRound starting_round_enum;
    if (starting_round_str == "Preflop") starting_round_enum = core::GameRound::kPreflop;
    else if (starting_round_str == "Flop") starting_round_enum = core::GameRound::kFlop;
    else if (starting_round_str == "Turn") starting_round_enum = core::GameRound::kTurn;
    else if (starting_round_str == "River") starting_round_enum = core::GameRound::kRiver;
    else throw std::invalid_argument("Invalid starting_round in JSON: " + starting_round_str);

    double initial_ip_commit = get_json_value_or_default(j_rule.value("initial_commitments", json::object()), "ip", 0.0);
    double initial_oop_commit = get_json_value_or_default(j_rule.value("initial_commitments", json::object()), "oop", 0.0);

    double sb = get_json_value_or_default(j_rule.value("blinds", json::object()), "sb", 0.0);
    double bb = get_json_value_or_default(j_rule.value("blinds", json::object()), "bb", 0.0);

    double effective_stack = j_rule.at("effective_stack").get<double>();
    int raise_limit = j_rule.at("raise_limit_per_street").get<int>();
    double all_in_thresh = get_json_value_or_default(j_rule, "all_in_threshold_ratio", 0.98);

    std::vector<int> initial_board_cards_for_rule; // << PARSE THIS
    if (j_rule.contains("initial_board") && j_rule.at("initial_board").is_array()) {
        for (const auto& card_str_json : j_rule.at("initial_board")) {
            std::string card_str = card_str_json.get<std::string>();
            std::optional<int> card_val = core::Card::StringToInt(card_str);
            if (!card_val) {
                throw std::invalid_argument("Invalid card string in initial_board for Rule: " + card_str);
            }
            initial_board_cards_for_rule.push_back(card_val.value());
        }
    }

    config::GameTreeBuildingSettings build_settings;
    if (j_rule.contains("building_settings") && j_rule.at("building_settings").is_object()) {
        const auto& j_bs = j_rule.at("building_settings");
        build_settings = config::GameTreeBuildingSettings(
            parse_street_setting(j_bs.at("flop_ip")),
            parse_street_setting(j_bs.at("turn_ip")),
            parse_street_setting(j_bs.at("river_ip")),
            parse_street_setting(j_bs.at("flop_oop")),
            parse_street_setting(j_bs.at("turn_oop")),
            parse_street_setting(j_bs.at("river_oop"))
        );
    }

    return config::Rule(deck, initial_oop_commit, initial_ip_commit, starting_round_enum,
                        initial_board_cards_for_rule, // <<< PASS PARSED BOARD CARDS
                        raise_limit, sb, bb, effective_stack, build_settings, all_in_thresh);
}
// --- End of Inline Helper Function Definitions ---

struct TestScenario {
    std::string test_case_name;
    std::string description;
    solver::PCfrSolver::Config solver_config;
    config::Rule game_rule;
    std::vector<core::PrivateCards> range_ip;
    std::vector<core::PrivateCards> range_oop;
    std::string expected_output_file;
    std::vector<int> initial_board_ints_for_pcm; // For PrivateCardsManager

    // Constructor declaration
    TestScenario(const nlohmann::json& j_scenario, const core::Deck& deck);
};

// Function declaration
TestScenario load_test_scenario(const std::string& scenario_filepath, const core::Deck& deck);

#endif // TEST_SCENARIO_LOADER_H