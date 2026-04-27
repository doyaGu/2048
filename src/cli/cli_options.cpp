#include "cli/cli_options.h"

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
    if (value == "ntuple") {
        return ai::AgentKind::Ntuple;
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
    if (arg == "inspect") {
        return CliCommand::Inspect;
    }
    if (arg == "train") {
        return CliCommand::Train;
    }
    if (arg == "matrix") {
        return CliCommand::Matrix;
    }
    if (arg == "microbench") {
        return CliCommand::Microbench;
    }
    if (arg == "parity") {
        return CliCommand::Parity;
    }
    if (arg == "analyze") {
        throw std::runtime_error("legacy analyze command is no longer supported; use inspect");
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
        if (first == "play" || first == "bench" || first == "inspect" || first == "train" ||
            first == "matrix" || first == "microbench" || first == "parity" || first == "analyze" ||
            (!first.empty() && first[0] != '-')) {
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

        if (arg == "--profile") {
            if (options.command != CliCommand::Train && options.command != CliCommand::Bench &&
                options.command != CliCommand::Matrix && options.command != CliCommand::Inspect &&
                options.command != CliCommand::Microbench && options.command != CliCommand::Parity) {
                throw std::runtime_error("--profile is only valid for train, bench, matrix, inspect, microbench, or parity");
            }
            options.profilePath = requireValue("--profile");
        } else if (arg == "--tdl-bin") {
            if (options.command != CliCommand::Parity) {
                throw std::runtime_error("--tdl-bin is only valid for parity");
            }
            options.tdlBinPath = requireValue("--tdl-bin");
        } else if (arg == "--games") {
            if (options.command != CliCommand::Microbench) {
                throw std::runtime_error("--games is only valid for microbench");
            }
            options.microbenchGames = static_cast<std::size_t>(std::stoull(requireValue("--games")));
        } else if (arg == "--weights") {
            if (options.command != CliCommand::Play && options.command != CliCommand::Bench &&
                options.command != CliCommand::Inspect) {
                throw std::runtime_error("--weights is only valid for play, bench, or inspect");
            }
            options.weightPath = requireValue("--weights");
        } else if (arg == "--max-jobs") {
            if (options.command != CliCommand::Matrix) {
                throw std::runtime_error("--max-jobs is only valid for matrix");
            }
            options.maxJobs = std::stoi(requireValue("--max-jobs"));
        } else if (arg == "--board") {
            if (options.command != CliCommand::Inspect) {
                throw std::runtime_error("--board is only valid for inspect");
            }
            options.boardRows = requireValue("--board");
        } else if (arg == "--seed") {
            if (options.command != CliCommand::Play && options.command != CliCommand::Inspect) {
                throw std::runtime_error("--seed is only valid for play or inspect");
            }
            options.seed = std::stoull(requireValue("--seed"));
        } else if (arg == "--ai") {
            if (options.command != CliCommand::Play) {
                throw std::runtime_error("--ai is only valid for play");
            }
            options.agent = ParseAgent(requireValue("--ai"));
        } else if (arg == "--depth") {
            if (options.command != CliCommand::Play) {
                throw std::runtime_error("--depth is only valid for play");
            }
            options.search.maxDepth = std::stoi(requireValue("--depth"));
        } else if (arg == "--time-budget-ms") {
            if (options.command != CliCommand::Play) {
                throw std::runtime_error("--time-budget-ms is only valid for play");
            }
            options.search.timeBudgetMs = std::stoi(requireValue("--time-budget-ms"));
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if ((options.command == CliCommand::Train || options.command == CliCommand::Bench ||
         options.command == CliCommand::Matrix || options.command == CliCommand::Microbench ||
         options.command == CliCommand::Parity) &&
        !options.profilePath.has_value()) {
        throw std::runtime_error("command requires --profile PATH");
    }
    if (options.command == CliCommand::Bench && !options.weightPath.has_value()) {
        throw std::runtime_error("bench requires --weights PATH");
    }
    if (options.command == CliCommand::Matrix && options.maxJobs < 1) {
        throw std::runtime_error("--max-jobs must be >= 1");
    }
    if (options.command == CliCommand::Microbench && options.microbenchGames == 0) {
        throw std::runtime_error("--games must be >= 1");
    }
    if (options.command == CliCommand::Inspect && options.boardRows.has_value() && options.boardRows->empty()) {
        throw std::runtime_error("inspect --board cannot be empty");
    }

    return options;
}

}  // namespace game2048
