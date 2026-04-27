#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "search/ai_engine.h"
#include "value/ntuple.h"

namespace game2048 {

enum class CliCommand {
    Play,
    Bench,
    Inspect,
    Train,
    Matrix,
    Microbench,
    Parity,
};

struct CliOptions {
    CliCommand command = CliCommand::Play;
    std::uint64_t seed = kDefaultSeed;
    ai::AgentKind agent = ai::AgentKind::Expectimax;
    ai::SearchConfig search {};
    std::optional<std::string> profilePath;
    std::optional<std::string> weightPath;
    std::optional<std::string> tdlBinPath;
    std::optional<std::string> boardRows;
    int maxJobs = 1;
    std::size_t microbenchGames = 200;
};

CliOptions ParseCliOptions(int argc, char** argv);

}  // namespace game2048
