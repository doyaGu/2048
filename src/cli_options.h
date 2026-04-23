#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "ai/ai_engine.h"

namespace game2048 {

struct CliOptions {
    std::optional<std::size_t> benchmarkGames;
    std::uint64_t seed = kDefaultSeed;
    ai::AgentKind agent = ai::AgentKind::Expectimax;
    ai::SearchConfig search {};
    std::optional<std::string> csvPath;
};

CliOptions ParseCliOptions(int argc, char** argv);

}  // namespace game2048
