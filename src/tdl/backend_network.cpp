#include "tdl/backend_network.h"

#include "tdl/backend_common.h"
#include "tdl/move_order.h"

namespace game2048::ai {

NetworkTdlBackend::NetworkTdlBackend(NtupleNetwork& network, LearningMode mode)
    : network_(network), mode_(mode) {}

TdlCandidateMove NetworkTdlBackend::ChooseBest(const FastBoard& board) const {
    return tdl_backend_detail::ChooseBestWithNetworkEvaluator(board, network_);
}

NtupleUpdateStats NetworkTdlBackend::Update(std::uint64_t bits, double target, double alpha) const {
    return network_.UpdateTowardFast(FastBoard(bits), target, alpha, mode_, false);
}

TdlCandidateMove NetworkEvalBackend::ChooseBest(const FastBoard& board) const {
    return tdl_backend_detail::ChooseBestWithNetworkEvaluator(board, network_);
}

namespace tdl_backend_detail {

TdlCandidateMove ChooseBestWithNetworkEvaluator(const FastBoard& board, const NtupleNetwork& network) {
    return ChooseBestByTdlOrder<TdlCandidateMove>(
        board,
        [&network](const FastMoveResult& move) {
            return network.Evaluate(FastBoard(move.board));
        },
        [](TdlCandidateMove& best, std::size_t index, const FastMoveResult& move, double value) {
            best.valid = true;
            best.direction = kTdlMoveDirections[index];
            best.move = move;
            best.value = value;
        });
}

}  // namespace tdl_backend_detail

}  // namespace game2048::ai
