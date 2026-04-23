#include "app/cli_options.h"

#include <stdexcept>

namespace game2048 {

CliOptions ParseCliOptions(int argc, char** argv) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto requireValue = [&](const char* name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++index];
        };

        if (arg == "--benchmark") {
            options.benchmarkGames = static_cast<std::size_t>(std::stoul(requireValue("--benchmark")));
        } else if (arg == "--seed") {
            options.seed = std::stoull(requireValue("--seed"));
        } else if (arg == "--ai") {
            const auto value = requireValue("--ai");
            if (value == "greedy") {
                options.agent = ai::AgentKind::Greedy;
            } else if (value == "expectimax") {
                options.agent = ai::AgentKind::Expectimax;
            } else {
                throw std::runtime_error("unknown AI: " + value);
            }
        } else if (arg == "--depth") {
            options.search.maxDepth = std::stoi(requireValue("--depth"));
        } else if (arg == "--time-budget-ms") {
            options.search.timeBudgetMs = std::stoi(requireValue("--time-budget-ms"));
        } else if (arg == "--csv") {
            options.csvPath = requireValue("--csv");
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    return options;
}

}  // namespace game2048
