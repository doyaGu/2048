#include "app/cli_options.h"

#include <stdexcept>

namespace game2048 {

namespace {

ai::AgentKind ParseAgent(const std::string& value) {
    if (value == "greedy") {
        return ai::AgentKind::Greedy;
    }
    if (value == "expectimax") {
        return ai::AgentKind::Expectimax;
    }
    throw std::runtime_error("unknown AI: " + value);
}

CliCommand ParseCommand(const std::string& arg) {
    if (arg == "play") {
        return CliCommand::Play;
    }
    if (arg == "bench") {
        return CliCommand::Bench;
    }
    if (arg == "analyze") {
        return CliCommand::Analyze;
    }
    if (!arg.empty() && arg[0] == '-') {
        throw std::runtime_error("missing subcommand before option: " + arg);
    }
    throw std::runtime_error("unknown subcommand: " + arg);
}

}  // namespace

CliOptions ParseCliOptions(int argc, char** argv) {
    CliOptions options;
    int index = 1;
    if (index < argc) {
        const std::string first = argv[index];
        if (first == "play" || first == "bench" || first == "analyze" || (!first.empty() && first[0] != '-')) {
            options.command = ParseCommand(first);
            ++index;
        }
    }

    for (; index < argc; ++index) {
        const std::string arg = argv[index];
        auto requireValue = [&](const char* name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++index];
        };

        if (arg == "--benchmark") {
            throw std::runtime_error("legacy --benchmark is no longer supported; use: game2048 bench --games N");
        } else if (arg == "--games") {
            if (options.command != CliCommand::Bench) {
                throw std::runtime_error("--games is only valid for bench");
            }
            options.benchmarkGames = static_cast<std::size_t>(std::stoul(requireValue("--games")));
        } else if (arg == "--seed") {
            options.seed = std::stoull(requireValue("--seed"));
        } else if (arg == "--ai") {
            options.agent = ParseAgent(requireValue("--ai"));
        } else if (arg == "--board") {
            if (options.command != CliCommand::Analyze) {
                throw std::runtime_error("--board is only valid for analyze");
            }
            options.boardRows = requireValue("--board");
        } else if (arg == "--depth") {
            options.search.maxDepth = std::stoi(requireValue("--depth"));
        } else if (arg == "--time-budget-ms") {
            options.search.timeBudgetMs = std::stoi(requireValue("--time-budget-ms"));
        } else if (arg == "--csv") {
            if (options.command != CliCommand::Bench) {
                throw std::runtime_error("--csv is only valid for bench");
            }
            options.csvPath = requireValue("--csv");
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.command == CliCommand::Bench && !options.benchmarkGames.has_value()) {
        throw std::runtime_error("bench requires --games N");
    }
    if (options.command == CliCommand::Analyze && options.boardRows.has_value() && options.boardRows->empty()) {
        throw std::runtime_error("analyze --board cannot be empty");
    }

    return options;
}

}  // namespace game2048
