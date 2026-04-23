#include "runtime/ai_worker.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace game2048 {

AIWorker::AIWorker()
    : thread_(&AIWorker::Run, this) {}

AIWorker::~AIWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void AIWorker::Configure(ai::AgentKind agent, const ai::SearchConfig& search) {
    std::lock_guard<std::mutex> lock(mutex_);
    agent_ = agent;
    search_ = search;
}

void AIWorker::Submit(const Board& board, std::uint64_t revision) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = Request {FastBoard::FromReference(board), revision, agent_, search_};
    }
    cv_.notify_one();
}

std::optional<AIWorkerResult> AIWorker::Poll() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = std::move(completed_);
    completed_.reset();
    return result;
}

bool AIWorker::Busy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ || pending_.has_value();
}

void AIWorker::Run() {
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return stop_ || pending_.has_value(); });
            if (stop_) {
                return;
            }
            request = *pending_;
            pending_.reset();
            running_ = true;
        }

        AIWorkerResult result;
        result.revision = request.revision;
        try {
            if (request.agent == ai::AgentKind::Expectimax && request.search.maxDepth < 0) {
                throw std::runtime_error("expectimax max depth must be non-negative");
            }
            ai::AIEngine engine;
            engine.SetAgent(request.agent);
            engine.Expectimax().SetConfig(request.search);
            result.decision = engine.Recommend(request.board);
        } catch (const std::exception& ex) {
            result.failed = true;
            result.error = ex.what();
        } catch (...) {
            result.failed = true;
            result.error = "unknown AI worker failure";
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            completed_ = std::move(result);
            running_ = false;
        }
    }
}

}  // namespace game2048
