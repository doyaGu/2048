#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "ai/ai_engine.h"
#include "core/board.h"
#include "core/board_fast.h"

namespace game2048 {

struct AIWorkerResult {
    std::uint64_t revision = 0;
    ai::MoveDecision decision {};
    bool failed = false;
    std::string error;
};

class AIWorker {
public:
    AIWorker();
    ~AIWorker();

    AIWorker(const AIWorker&) = delete;
    AIWorker& operator=(const AIWorker&) = delete;

    void Configure(ai::AgentKind agent, const ai::SearchConfig& search);
    void Submit(const Board& board, std::uint64_t revision);
    std::optional<AIWorkerResult> Poll();
    bool Busy() const;

private:
    struct Request {
        FastBoard board {};
        std::uint64_t revision = 0;
        ai::AgentKind agent = ai::AgentKind::Expectimax;
        ai::SearchConfig search {};
    };

    void Run();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    bool running_ = false;
    ai::AgentKind agent_ = ai::AgentKind::Expectimax;
    ai::SearchConfig search_ {};
    std::optional<Request> pending_ {};
    std::optional<AIWorkerResult> completed_ {};
    std::thread thread_;
};

}  // namespace game2048
