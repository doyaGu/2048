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

std::uint64_t AIWorker::Configure(ai::AgentKind agent, const ai::SearchConfig& search,
                                  std::shared_ptr<const ai::NtupleNetwork> ntupleNetwork) {
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        agent_ = agent;
        search_ = search;
        ntupleNetwork_ = std::move(ntupleNetwork);
        ++generation_;
        generation = generation_;
        pending_.reset();
        completed_.reset();
    }
    cv_.notify_one();
    return generation;
}

void AIWorker::Submit(const Board& board, std::uint64_t revision) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = Request {FastBoard::FromReference(board), revision, generation_, agent_, search_, ntupleNetwork_};
    }
    cv_.notify_one();
}

std::optional<AIWorkerResult> AIWorker::Poll() {
    std::optional<AIWorkerResult> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result = std::move(completed_);
        completed_.reset();
    }
    if (result.has_value()) {
        cv_.notify_one();
    }
    return result;
}

bool AIWorker::Busy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ || pending_.has_value() || completed_.has_value();
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
        result.generation = request.generation;
        try {
            if (request.agent == ai::AgentKind::Expectimax && request.search.maxDepth < 0) {
                throw std::runtime_error("expectimax max depth must be non-negative");
            }
            ai::AIEngine engine;
            engine.SetAgent(request.agent);
            engine.Expectimax().SetConfig(request.search);
            if (request.ntupleNetwork) {
                if (request.agent == ai::AgentKind::Expectimax) {
                    engine.Expectimax().SetLeafNetworkShared(request.ntupleNetwork);
                    engine.Expectimax().SetLeafPriorWeight(0.0);
                } else if (request.agent == ai::AgentKind::Ntuple) {
                    engine.Ntuple().SetNetworkShared(request.ntupleNetwork);
                }
            }
            result.decision = engine.Recommend(request.board);
        } catch (const std::exception& ex) {
            result.failed = true;
            result.error = ex.what();
        } catch (...) {
            result.failed = true;
            result.error = "unknown AI worker failure";
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return stop_ || !completed_.has_value(); });
            if (stop_) {
                return;
            }
            completed_ = std::move(result);
            running_ = false;
        }
    }
}

}  // namespace game2048
