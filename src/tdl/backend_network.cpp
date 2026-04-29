#include "tdl/backend_network.h"

#include <array>
#include <limits>

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
    TdlCandidateMove best;
    best.value = -std::numeric_limits<double>::infinity();
    std::array<FastMoveResult, 4> moves {};
    board.TdlOrderMoves(moves);
    for (std::size_t index = 0; index < moves.size(); ++index) {
        const FastMoveResult& move = moves[index];
        if (!move.changed) {
            continue;
        }
        const FastBoard afterstate(move.board);
        const double value = static_cast<double>(move.scoreDelta) + network.Evaluate(afterstate);
        if (!best.valid || value > best.value) {
            best.valid = true;
            best.direction = kTdlMoveDirections[index];
            best.move = move;
            best.value = value;
        }
    }
    return best;
}

}  // namespace tdl_backend_detail

}  // namespace game2048::ai
