#include "value/ntuple.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "value/ntuple_kernel.h"

namespace game2048::ai {

namespace {

constexpr std::string_view kNtupleMagicV5 = "G2048NT5";
constexpr std::uint32_t kNtupleVersionV5 = 5;
constexpr std::uint32_t kNtupleEndianMarker = 0x01020304U;
constexpr std::uint32_t kNtupleHeaderBytes = 32;
constexpr std::uint32_t kNtupleFlags = 0;
constexpr std::uint32_t kNtupleValueFloat32 = 1;
constexpr std::uint32_t kNtupleKeyBitsPerCell = 4;
constexpr std::string_view kChunkMeta = "META";
constexpr std::string_view kChunkPatterns = "PATT";
constexpr std::string_view kChunkStages = "STAG";
constexpr std::string_view kChunkDense = "DENS";
constexpr std::string_view kChunkTc = "TCST";
constexpr std::string_view kChunkTcSparse = "TCS2";
constexpr float kTcInitial = std::numeric_limits<float>::min();

template <typename T>
void WriteBinary(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
T ReadBinary(std::istream& in) {
    T value {};
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!in) {
        throw std::runtime_error("truncated n-tuple binary payload");
    }
    return value;
}

void ReadExact(std::istream& in, char* data, std::size_t size) {
    if (size == 0) {
        return;
    }
    in.read(data, static_cast<std::streamsize>(size));
    if (!in) {
        throw std::runtime_error("truncated n-tuple binary payload");
    }
}

void WriteChunkHeader(std::ostream& out, std::string_view id, std::uint64_t size) {
    out.write(id.data(), static_cast<std::streamsize>(id.size()));
    WriteBinary(out, size);
}

std::string ReadChunkId(std::istream& in) {
    std::array<char, 4> id {};
    in.read(id.data(), static_cast<std::streamsize>(id.size()));
    if (!in) {
        throw std::runtime_error("truncated n-tuple chunk id");
    }
    return std::string(id.begin(), id.end());
}

bool IsChunk(const std::string& actual, std::string_view expected) {
    return actual.size() == expected.size() &&
           std::equal(actual.begin(), actual.end(), expected.begin());
}

int TransformCell(int cell, int transform) {
    const int row = cell / kBoardSize;
    const int col = cell % kBoardSize;
    int nextRow = row;
    int nextCol = col;
    switch (transform) {
        case 0:
            break;
        case 1:
            nextRow = col;
            nextCol = kBoardSize - 1 - row;
            break;
        case 2:
            nextRow = kBoardSize - 1 - row;
            nextCol = kBoardSize - 1 - col;
            break;
        case 3:
            nextRow = kBoardSize - 1 - col;
            nextCol = row;
            break;
        case 4:
            nextRow = row;
            nextCol = kBoardSize - 1 - col;
            break;
        case 5:
            nextRow = kBoardSize - 1 - row;
            nextCol = col;
            break;
        case 6:
            nextRow = col;
            nextCol = row;
            break;
        case 7:
            nextRow = kBoardSize - 1 - col;
            nextCol = kBoardSize - 1 - row;
            break;
    }
    return nextRow * kBoardSize + nextCol;
}

int InverseTransformCell(int cell, int transform) {
    for (int candidate = 0; candidate < kCellCount; ++candidate) {
        if (TransformCell(candidate, transform) == cell) {
            return candidate;
        }
    }
    return cell;
}

void AddPatternIfMissing(std::vector<NtuplePattern>& patterns, NtuplePattern pattern) {
    const auto exists = std::find_if(patterns.begin(), patterns.end(), [&](const NtuplePattern& existing) {
        return existing.cells == pattern.cells;
    });
    if (exists == patterns.end()) {
        patterns.push_back(std::move(pattern));
    }
}

FastMoveResult ApplyDirection(const FastBoard& board, Direction direction) {
    switch (direction) {
        case Direction::Left:
            return board.MoveLeft();
        case Direction::Right:
            return board.MoveRight();
        case Direction::Up:
            return board.MoveUp();
        case Direction::Down:
            return board.MoveDown();
    }
    return {};
}

double CombinedAfterstateValue(const FastBoard& board, const NtupleNetwork& network,
                               const Evaluator& evaluator, double priorWeight) {
    if (priorWeight == 0.0) {
        return network.Evaluate(board);
    }
    return network.Evaluate(board) + priorWeight * evaluator.Evaluate(board);
}

MoveDecision ChooseNtupleMove(const FastBoard& board, const NtupleNetwork& network,
                              const Evaluator& evaluator, double priorWeight) {
    static constexpr std::array<Direction, 4> kDirections {
        Direction::Left,
        Direction::Up,
        Direction::Right,
        Direction::Down,
    };

    MoveDecision best;
    best.value = -std::numeric_limits<double>::infinity();
    SearchStats stats;

    for (Direction direction : kDirections) {
        const FastMoveResult move = ApplyDirection(board, direction);
        if (!move.changed) {
            continue;
        }

        ++stats.nodes;
        const double value = static_cast<double>(move.scoreDelta) +
                             CombinedAfterstateValue(FastBoard(move.board), network, evaluator, priorWeight);
        if (!best.valid || value > best.value) {
            best.valid = true;
            best.direction = direction;
            best.value = value;
        }
    }

    best.stats = stats;
    best.stats.evaluation = best.value;
    return best;
}

double BestMoveValueAfterSpawn(const FastBoard& board, const NtupleNetwork& network,
                               const Evaluator& evaluator, double priorWeight) {
    const MoveDecision decision = ChooseNtupleMove(board, network, evaluator, priorWeight);
    return decision.valid ? decision.value : 0.0;
}

double ExpectedSpawnTarget(const FastBoard& afterMove, const NtupleNetwork& network,
                           const Evaluator& evaluator, double priorWeight) {
    const auto empties = afterMove.EmptyIndices();
    if (empties.empty()) {
        return BestMoveValueAfterSpawn(afterMove, network, evaluator, priorWeight);
    }

    const double cellProbability = 1.0 / static_cast<double>(empties.size());
    double expected = 0.0;
    for (int index : empties) {
        FastBoard spawned2 = afterMove;
        spawned2.SetRank(index, 1);
        expected += cellProbability * (1.0 - kSpawnProbability4) *
                    BestMoveValueAfterSpawn(spawned2, network, evaluator, priorWeight);

        FastBoard spawned4 = afterMove;
        spawned4.SetRank(index, 2);
        expected += cellProbability * kSpawnProbability4 *
                    BestMoveValueAfterSpawn(spawned4, network, evaluator, priorWeight);
    }
    return expected;
}

bool UsesTc(LearningMode mode) {
    return mode == LearningMode::TC || mode == LearningMode::OptimisticTC;
}

NtupleTrainingStats ApplyBackwardAfterstateTraceInternal(NtupleNetwork& network,
                                                         const std::vector<NtupleTraceStep>& trace,
                                                         double alpha,
                                                         LearningMode mode,
                                                         double priorWeight) {
    NtupleTrainingStats stats;
    stats.games = trace.empty() ? 0U : 1U;
    stats.moves = trace.size();
    stats.stageUpdates.assign(std::max<std::size_t>(1, network.StageCount()), 0);
    std::optional<Evaluator> evaluator;
    if (priorWeight != 0.0) {
        evaluator.emplace();
    }

    double absErrorSum = 0.0;
    double squaredErrorSum = 0.0;
    double nextValue = 0.0;
    double nextReward = 0.0;
    for (auto it = trace.rbegin(); it != trace.rend(); ++it) {
        const FastBoard afterstate = it->Afterstate();
        const double target = nextReward + nextValue;
        const double residualTarget = priorWeight == 0.0
            ? target
            : target - priorWeight * evaluator->Evaluate(afterstate);
        const std::size_t stage = network.StageFor(afterstate);
        if (stage >= stats.stageUpdates.size()) {
            stats.stageUpdates.resize(stage + 1U, 0);
        }
        const NtupleUpdateStats update = network.UpdateTowardFast(afterstate, residualTarget, alpha, mode);
        const double absError = std::abs(update.error);
        absErrorSum += absError;
        squaredErrorSum += update.error * update.error;
        stats.maxAbsTdError = std::max(stats.maxAbsTdError, absError);
        ++stats.stageUpdates[stage];
        ++stats.updates;
        stats.totalScore += it->reward;
        stats.maxTile = std::max(stats.maxTile, afterstate.MaxTile());

        nextValue = priorWeight == 0.0
            ? update.after
            : update.after + priorWeight * evaluator->Evaluate(afterstate);
        nextReward = static_cast<double>(it->reward);
    }

    if (stats.updates > 0) {
        stats.meanAbsTdError = absErrorSum / static_cast<double>(stats.updates);
        stats.rmsTdError = std::sqrt(squaredErrorSum / static_cast<double>(stats.updates));
    }
    stats.tcTouchedEntries = network.TouchedTcEntries();
    return stats;
}

FastBoard MakeStageStartBoard(int startRank, Random& rng) {
    FastBoard board;
    if (startRank <= 0) {
        rng.SpawnOnFastBoard(board);
        rng.SpawnOnFastBoard(board);
        return board;
    }

    static constexpr std::array<int, 12> kSnakeCells {
        0, 1, 2, 3,
        7, 6, 5, 4,
        8, 9, 10, 11,
    };
    const int filled = std::min<int>(static_cast<int>(kSnakeCells.size()), startRank);
    for (int index = 0; index < filled; ++index) {
        board.SetRank(kSnakeCells[static_cast<std::size_t>(index)], std::max(1, startRank - index));
    }
    const int transform = static_cast<int>(rng.NextIndex(8));
    board = board.TransformD4(transform);
    rng.SpawnOnFastBoard(board);
    rng.SpawnOnFastBoard(board);
    return board;
}

std::size_t EffectiveFeatureCount(const NtuplePatternSet& patternSet) {
    const std::size_t transforms = patternSet.useD4 ? 8U : 1U;
    return std::max<std::size_t>(1, patternSet.basePatterns.size() * transforms);
}

}  // namespace

NtuplePatternSet PatternSetForPreset(NtuplePreset preset) {
    if (preset == NtuplePreset::Tdl4x6Khyeh) {
        return {
            "tdl-4x6-khyeh",
            {
                {{0, 1, 2, 3, 4, 5}},
                {{4, 5, 6, 7, 8, 9}},
                {{0, 1, 2, 4, 5, 6}},
                {{4, 5, 6, 8, 9, 10}},
            },
            true,
            true,
            16,
        };
    }
    if (preset == NtuplePreset::Tdl8x6KMatsuzaki) {
        return {
            "tdl-8x6-kmatsuzaki",
            {
                {{0, 1, 2, 4, 5, 6}},
                {{4, 5, 6, 7, 8, 9}},
                {{0, 1, 2, 3, 4, 5}},
                {{2, 3, 4, 5, 6, 9}},
                {{0, 1, 2, 5, 9, 10}},
                {{3, 4, 5, 6, 7, 8}},
                {{1, 3, 4, 5, 6, 7}},
                {{0, 1, 4, 8, 9, 10}},
            },
            true,
            true,
            16,
        };
    }
    return {
        "compact-d4",
        {
            {{0, 1, 2, 3}},
            {{0, 1, 5, 4}},
            {{0, 1, 2, 6}},
            {{0, 4, 5, 6}},
        },
        true,
        false,
        16,
    };
}

std::vector<NtuplePattern> PatternsForPreset(NtuplePreset preset) {
    return PatternSetForPreset(preset).basePatterns;
}

std::vector<int> DefaultStageBoundaries() {
    return {11, 12, 13, 14, 15};
}

std::vector<NtuplePattern> DefaultNtuplePatterns() {
    const std::array<NtuplePattern, 4> seeds {{
        {{0, 1, 2, 3}},
        {{0, 1, 5, 4}},
        {{0, 1, 2, 6}},
        {{0, 4, 5, 6}},
    }};

    std::vector<NtuplePattern> patterns;
    patterns.reserve(32);
    for (const auto& seed : seeds) {
        for (int transform = 0; transform < 8; ++transform) {
            NtuplePattern pattern;
            pattern.cells.reserve(seed.cells.size());
            for (int cell : seed.cells) {
                pattern.cells.push_back(TransformCell(cell, transform));
            }
            AddPatternIfMissing(patterns, std::move(pattern));
        }
    }
    return patterns;
}

NtupleNetwork::NtupleNetwork()
    : NtupleNetwork(DefaultNtuplePatterns()) {}

NtupleNetwork::NtupleNetwork(std::vector<NtuplePattern> patterns,
                             float optimisticInit,
                             std::vector<int> stageBoundaries)
    : patternSet_({"custom", patterns, false, false, 16}),
      patterns_(std::move(patterns)),
      stageBoundaries_(std::move(stageBoundaries)) {
    patternSet_.basePatterns = patterns_;
    patternOffsets_.reserve(patterns_.size());
    for (const auto& pattern : patterns_) {
        patternOffsets_.push_back(entriesPerStage_);
        entriesPerStage_ += EntriesForPattern(pattern);
    }
    BuildFixedPathCache();

    const std::size_t stageCount = stageBoundaries_.size() + 1U;
    stageValues_.resize(stageCount);
    tc_.resize(stageCount);
    promotedStages_.assign(stageCount, false);
    EnsureStage(0);
    std::fill(stageValues_[0].begin(), stageValues_[0].end(),
              optimisticInit / static_cast<float>(EffectiveFeatureCount(patternSet_)));
}

NtupleNetwork::NtupleNetwork(NtuplePatternSet patternSet,
                             float optimisticInit,
                             std::vector<int> stageBoundaries)
    : patternSet_(std::move(patternSet)),
      patterns_(patternSet_.basePatterns),
      stageBoundaries_(std::move(stageBoundaries)),
      profileMetadata_("preset=" + patternSet_.name) {
    patternOffsets_.reserve(patterns_.size());
    for (const auto& pattern : patterns_) {
        patternOffsets_.push_back(entriesPerStage_);
        entriesPerStage_ += EntriesForPattern(pattern);
    }
    BuildFixedPathCache();

    const std::size_t stageCount = stageBoundaries_.size() + 1U;
    stageValues_.resize(stageCount);
    tc_.resize(stageCount);
    promotedStages_.assign(stageCount, false);
    EnsureStage(0);
    std::fill(stageValues_[0].begin(), stageValues_[0].end(),
              optimisticInit / static_cast<float>(EffectiveFeatureCount(patternSet_)));
}

double NtupleNetwork::Evaluate(const FastBoard& board) const {
    const std::size_t stage = StageForRead(StageFor(board));
    if (patternSet_.useFixedPath && !fixedShifts6_.empty()) {
        return EvaluateFixedPath(stage, board);
    }
    float value = 0.0F;
    for (std::size_t index = 0; index < patterns_.size(); ++index) {
        if (patternSet_.useD4) {
            for (int transform = 0; transform < 8; ++transform) {
                const FastBoard transformed = board.TransformD4(transform);
                value += WeightAt(stage, patternOffsets_[index] + KeyFor(transformed, patterns_[index]));
            }
        } else {
            value += WeightAt(stage, patternOffsets_[index] + KeyFor(board, patterns_[index]));
        }
    }
    return static_cast<double>(value);
}

std::vector<std::size_t> NtupleNetwork::FeatureKeysForBoard(const FastBoard& board) const {
    std::vector<std::size_t> keys;
    keys.reserve(patterns_.size() * (patternSet_.useD4 ? 8U : 1U));
    for (std::size_t index = 0; index < patterns_.size(); ++index) {
        if (patternSet_.useD4) {
            for (int transform = 0; transform < 8; ++transform) {
                const FastBoard transformed = board.TransformD4(transform);
                keys.push_back(patternOffsets_[index] + KeyFor(transformed, patterns_[index]));
            }
        } else {
            keys.push_back(patternOffsets_[index] + KeyFor(board, patterns_[index]));
        }
    }
    return keys;
}

NtupleUpdateStats NtupleNetwork::UpdateToward(const FastBoard& board, double target, double alpha) {
    return UpdateToward(board, target, alpha, LearningMode::TD);
}

NtupleUpdateStats NtupleNetwork::UpdateToward(const FastBoard& board, double target, double alpha,
                                             LearningMode mode) {
    return UpdateTowardFast(board, target, alpha, mode);
}

NtupleUpdateStats NtupleNetwork::UpdateTowardFast(const FastBoard& board, double target, double alpha,
                                                 LearningMode mode, bool computeAfter) {
    const std::size_t stage = StageFor(board);
    if (stage > 0 && !promotedStages_[stage]) {
        PromoteStageFromPrevious(stage);
    }

    if (patterns_.empty()) {
        return {0.0, 0.0, target};
    }

    const std::size_t accessCount = patterns_.size() * (patternSet_.useD4 ? 8U : 1U);
    if (patternSet_.useFixedPath && !fixedShifts6_.empty()) {
        std::array<std::size_t, 128> fixedKeys {};
        auto& weights = stageValues_[stage];
        double before = 0.0;
        const std::size_t keyCount =
            ntuple_kernel::CollectFixed6KeysAndValue(board.Bits(), weights.data(), fixedOffsets_.data(),
                                                     fixedShifts6_.data(), fixedOffsets_.size(),
                                                     fixedKeys.data(), before);
        const float error = static_cast<float>(target) - static_cast<float>(before);
        const float delta = static_cast<float>(alpha) * error / static_cast<float>(keyCount);
        ApplyCollectedWeightDeltas(stage, fixedKeys.data(), keyCount, delta, mode);
        float after = 0.0F;
        if (computeAfter) {
            for (std::size_t index = 0; index < keyCount; ++index) {
                after += weights[fixedKeys[index]];
            }
        }
        return {before, after, error};
    }

    const double before = Evaluate(board);
    const float error = static_cast<float>(target) - static_cast<float>(before);
    const float delta = static_cast<float>(alpha) * error / static_cast<float>(accessCount);
    for (std::size_t index = 0; index < patterns_.size(); ++index) {
        const int transforms = patternSet_.useD4 ? 8 : 1;
        for (int transform = 0; transform < transforms; ++transform) {
            const FastBoard transformed = patternSet_.useD4 ? board.TransformD4(transform) : board;
            const std::size_t weightIndex = patternOffsets_[index] + KeyFor(transformed, patterns_[index]);
            ApplyWeightDelta(stage, weightIndex, delta, mode);
        }
    }
    return {before, computeAfter ? Evaluate(board) : 0.0, error};
}

void NtupleNetwork::Save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open n-tuple output: " + path);
    }

    out.write(kNtupleMagicV5.data(), static_cast<std::streamsize>(kNtupleMagicV5.size()));
    WriteBinary(out, kNtupleVersionV5);
    WriteBinary(out, kNtupleEndianMarker);
    WriteBinary(out, kNtupleHeaderBytes);
    WriteBinary(out, kNtupleFlags);
    WriteBinary(out, kNtupleValueFloat32);
    WriteBinary(out, kNtupleKeyBitsPerCell);

    {
        std::ostringstream chunk(std::ios::binary);
        WriteBinary(chunk, static_cast<std::uint64_t>(profileMetadata_.size()));
        chunk.write(profileMetadata_.data(), static_cast<std::streamsize>(profileMetadata_.size()));
        WriteBinary(chunk, static_cast<std::uint32_t>(patternSet_.useD4 ? 1U : 0U));
        WriteBinary(chunk, static_cast<std::uint32_t>(patternSet_.useFixedPath ? 1U : 0U));
        WriteBinary(chunk, static_cast<std::uint32_t>(patternSet_.entryRadix));
        const std::string payload = chunk.str();
        WriteChunkHeader(out, kChunkMeta, payload.size());
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    {
        std::ostringstream chunk(std::ios::binary);
        WriteBinary(chunk, static_cast<std::uint64_t>(patterns_.size()));
        for (const auto& pattern : patterns_) {
            WriteBinary(chunk, static_cast<std::uint64_t>(pattern.cells.size()));
            for (int cell : pattern.cells) {
                WriteBinary(chunk, static_cast<std::int32_t>(cell));
            }
        }
        const std::string payload = chunk.str();
        WriteChunkHeader(out, kChunkPatterns, payload.size());
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    {
        std::ostringstream chunk(std::ios::binary);
        WriteBinary(chunk, static_cast<std::uint64_t>(stageBoundaries_.size()));
        WriteBinary(chunk, static_cast<std::uint64_t>(StageCount()));
        WriteBinary(chunk, static_cast<std::uint64_t>(entriesPerStage_));
        for (int boundary : stageBoundaries_) {
            WriteBinary(chunk, static_cast<std::int32_t>(boundary));
        }
        for (bool promoted : promotedStages_) {
            WriteBinary(chunk, static_cast<std::uint32_t>(promoted ? 1U : 0U));
        }
        const std::string payload = chunk.str();
        WriteChunkHeader(out, kChunkStages, payload.size());
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    {
        std::ostringstream chunk(std::ios::binary);
        WriteBinary(chunk, static_cast<std::uint64_t>(stageValues_.size()));
        for (const auto& values : stageValues_) {
            WriteBinary(chunk, static_cast<std::uint64_t>(values.size()));
            if (!values.empty()) {
                chunk.write(reinterpret_cast<const char*>(values.data()),
                            static_cast<std::streamsize>(values.size() * sizeof(float)));
            }
        }
        const std::string payload = chunk.str();
        WriteChunkHeader(out, kChunkDense, payload.size());
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    if (TouchedTcEntries() > 0) {
        std::ostringstream chunk(std::ios::binary);
        WriteBinary(chunk, static_cast<std::uint64_t>(tc_.size()));
        for (const auto& tcStage : tc_) {
            WriteBinary(chunk, static_cast<std::uint64_t>(tcStage.size()));
            std::vector<std::pair<std::size_t, TcEntry>> entries(tcStage.begin(), tcStage.end());
            std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.first < rhs.first;
            });
            for (const auto& [index, entry] : entries) {
                WriteBinary(chunk, static_cast<std::uint64_t>(index));
                WriteBinary(chunk, entry.accum);
                WriteBinary(chunk, entry.updvu);
            }
        }
        const std::string payload = chunk.str();
        WriteChunkHeader(out, kChunkTcSparse, payload.size());
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    if (!out) {
        throw std::runtime_error("failed to write n-tuple weights: " + path);
    }
}

const std::vector<NtuplePattern>& NtupleNetwork::Patterns() const {
    return patterns_;
}

std::size_t NtupleNetwork::WeightCount() const {
    std::size_t count = 0;
    for (const auto& values : stageValues_) {
        count += values.size();
    }
    return count;
}

std::size_t NtupleNetwork::StageCount() const {
    return stageBoundaries_.size() + 1U;
}

std::size_t NtupleNetwork::StageFor(const FastBoard& board) const {
    if (stageBoundaries_.empty()) {
        return 0;
    }
    const int rank = board.MaxRank();
    for (std::size_t index = 0; index < stageBoundaries_.size(); ++index) {
        if (rank < stageBoundaries_[index]) {
            return index;
        }
    }
    return stageBoundaries_.size();
}

void NtupleNetwork::PromoteStageFromPrevious(std::size_t stage) {
    if (stage == 0 || stage >= StageCount() || promotedStages_[stage]) {
        return;
    }
    if (!promotedStages_[stage - 1]) {
        PromoteStageFromPrevious(stage - 1);
    }
    EnsureStage(stage);
    stageValues_[stage] = stageValues_[stage - 1];
    promotedStages_[stage] = true;
}

void NtupleNetwork::EnableStages(std::vector<int> stageBoundaries) {
    if (stageBoundaries_ == stageBoundaries) {
        return;
    }
    if (StageCount() > 1U) {
        throw std::runtime_error("cannot replace existing multistage n-tuple boundaries");
    }
    stageBoundaries_ = std::move(stageBoundaries);
    stageValues_.resize(StageCount());
    tc_.assign(StageCount(), {});
    promotedStages_.assign(StageCount(), false);
    EnsureStage(0);
}

int NtupleNetwork::StageUpperRank(std::size_t stage) const {
    if (stage < stageBoundaries_.size()) {
        return stageBoundaries_[stage];
    }
    return 15;
}

std::size_t NtupleNetwork::TouchedTcEntries() const {
    std::size_t count = 0;
    for (const auto& stageTc : tc_) {
        count += stageTc.size();
    }
    return count;
}

bool NtupleNetwork::HasTcState() const {
    return TouchedTcEntries() > 0;
}

const std::string& NtupleNetwork::ProfileMetadata() const {
    return profileMetadata_;
}

void NtupleNetwork::SetProfileMetadata(std::string metadata) {
    profileMetadata_ = std::move(metadata);
}

const NtuplePatternSet& NtupleNetwork::PatternSet() const {
    return patternSet_;
}

NtupleFixed6View NtupleNetwork::Fixed6SingleStageView(LearningMode mode) const {
    if (mode != LearningMode::TD || StageCount() != 1U || !patternSet_.useFixedPath ||
        fixedShifts6_.empty() || fixedOffsets_.empty() || stageValues_.empty() || stageValues_[0].empty()) {
        return {};
    }
    return {stageValues_[0].data(), fixedOffsets_.data(), fixedShifts6_.data(), fixedOffsets_.size(), true};
}

NtupleMutableFixed6View NtupleNetwork::MutableFixed6SingleStageView(LearningMode mode) {
    if (mode != LearningMode::TD || StageCount() != 1U || !patternSet_.useFixedPath ||
        fixedShifts6_.empty() || fixedOffsets_.empty()) {
        return {};
    }
    EnsureStage(0);
    if (stageValues_.empty() || stageValues_[0].empty()) {
        return {};
    }
    return {stageValues_[0].data(), fixedOffsets_.data(), fixedShifts6_.data(), fixedOffsets_.size(), true};
}

NtupleNetwork NtupleNetwork::Load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open n-tuple input: " + path);
    }

    std::string magic(kNtupleMagicV5.size(), '\0');
    ReadExact(in, magic.data(), magic.size());
    if (magic != kNtupleMagicV5) {
        throw std::runtime_error("invalid n-tuple file: " + path);
    }

    const std::uint32_t version = ReadBinary<std::uint32_t>(in);
    const std::uint32_t endianMarker = ReadBinary<std::uint32_t>(in);
    const std::uint32_t headerBytes = ReadBinary<std::uint32_t>(in);
    const std::uint32_t flags = ReadBinary<std::uint32_t>(in);
    const std::uint32_t valueType = ReadBinary<std::uint32_t>(in);
    const std::uint32_t keyBits = ReadBinary<std::uint32_t>(in);
    if (version != kNtupleVersionV5 || endianMarker != kNtupleEndianMarker ||
        headerBytes != kNtupleHeaderBytes || flags != kNtupleFlags ||
        valueType != kNtupleValueFloat32 || keyBits != kNtupleKeyBitsPerCell) {
        throw std::runtime_error("invalid n-tuple version: " + path);
    }

    std::string metadata;
    bool useD4 = false;
    bool useFixedPath = false;
    int entryRadix = 16;
    std::vector<NtuplePattern> patterns;
    std::vector<int> boundaries;
    std::vector<bool> promoted;
    std::vector<std::vector<float>> denseStages;
    std::vector<NtupleNetwork::TcStage> tcStages;
    std::uint64_t entriesPerStage = 0;
    std::uint64_t stageCount = 0;

    while (in.peek() != EOF) {
        const std::string chunkId = ReadChunkId(in);
        const std::uint64_t chunkSize = ReadBinary<std::uint64_t>(in);
        std::string payload(chunkSize, '\0');
        ReadExact(in, payload.data(), payload.size());
        std::istringstream chunk(payload, std::ios::binary);
        if (IsChunk(chunkId, kChunkMeta)) {
            const std::uint64_t size = ReadBinary<std::uint64_t>(chunk);
            metadata.resize(static_cast<std::size_t>(size));
            ReadExact(chunk, metadata.data(), metadata.size());
            useD4 = ReadBinary<std::uint32_t>(chunk) != 0U;
            useFixedPath = ReadBinary<std::uint32_t>(chunk) != 0U;
            entryRadix = static_cast<int>(ReadBinary<std::uint32_t>(chunk));
        } else if (IsChunk(chunkId, kChunkPatterns)) {
            const std::uint64_t patternCount = ReadBinary<std::uint64_t>(chunk);
            patterns.resize(static_cast<std::size_t>(patternCount));
            for (auto& pattern : patterns) {
                const std::uint64_t length = ReadBinary<std::uint64_t>(chunk);
                pattern.cells.resize(static_cast<std::size_t>(length));
                for (int& cell : pattern.cells) {
                    cell = static_cast<int>(ReadBinary<std::int32_t>(chunk));
                }
            }
        } else if (IsChunk(chunkId, kChunkStages)) {
            const std::uint64_t boundaryCount = ReadBinary<std::uint64_t>(chunk);
            stageCount = ReadBinary<std::uint64_t>(chunk);
            entriesPerStage = ReadBinary<std::uint64_t>(chunk);
            boundaries.resize(static_cast<std::size_t>(boundaryCount));
            for (int& boundary : boundaries) {
                boundary = static_cast<int>(ReadBinary<std::int32_t>(chunk));
            }
            promoted.resize(static_cast<std::size_t>(stageCount));
            for (auto&& flag : promoted) {
                flag = ReadBinary<std::uint32_t>(chunk) != 0U;
            }
        } else if (IsChunk(chunkId, kChunkDense)) {
            const std::uint64_t count = ReadBinary<std::uint64_t>(chunk);
            denseStages.resize(static_cast<std::size_t>(count));
            for (auto& values : denseStages) {
                const std::uint64_t valueCount = ReadBinary<std::uint64_t>(chunk);
                values.resize(static_cast<std::size_t>(valueCount));
                if (!values.empty()) {
                    ReadExact(chunk, reinterpret_cast<char*>(values.data()), values.size() * sizeof(float));
                }
            }
        } else if (IsChunk(chunkId, kChunkTc)) {
            const std::uint64_t count = ReadBinary<std::uint64_t>(chunk);
            tcStages.resize(static_cast<std::size_t>(count));
            for (auto& stageTc : tcStages) {
                const std::uint64_t valueCount = ReadBinary<std::uint64_t>(chunk);
                for (std::uint64_t index = 0; index < valueCount; ++index) {
                    TcEntry entry;
                    ReadExact(chunk, reinterpret_cast<char*>(&entry), sizeof(TcEntry));
                    if (entry.updvu != kTcInitial) {
                        stageTc.emplace(static_cast<std::size_t>(index), entry);
                    }
                }
            }
        } else if (IsChunk(chunkId, kChunkTcSparse)) {
            const std::uint64_t count = ReadBinary<std::uint64_t>(chunk);
            tcStages.resize(static_cast<std::size_t>(count));
            for (auto& stageTc : tcStages) {
                const std::uint64_t valueCount = ReadBinary<std::uint64_t>(chunk);
                stageTc.reserve(static_cast<std::size_t>(valueCount));
                for (std::uint64_t offset = 0; offset < valueCount; ++offset) {
                    const std::uint64_t index = ReadBinary<std::uint64_t>(chunk);
                    TcEntry entry;
                    entry.accum = ReadBinary<float>(chunk);
                    entry.updvu = ReadBinary<float>(chunk);
                    if (entry.updvu != kTcInitial) {
                        stageTc.emplace(static_cast<std::size_t>(index), entry);
                    }
                }
            }
        }
    }

    if (patterns.empty() || denseStages.empty()) {
        throw std::runtime_error("missing n-tuple V5 chunks: " + path);
    }
    NtuplePatternSet set {metadata.empty() ? "loaded" : metadata, patterns, useD4, useFixedPath, entryRadix};
    NtupleNetwork network(std::move(set), 0.0F, std::move(boundaries));
    if (stageCount != network.StageCount() || entriesPerStage != network.entriesPerStage_) {
        throw std::runtime_error("n-tuple shape mismatch: " + path);
    }
    network.profileMetadata_ = std::move(metadata);
    network.stageValues_ = std::move(denseStages);
    network.tc_ = std::move(tcStages);
    if (network.tc_.size() < network.StageCount()) {
        network.tc_.resize(network.StageCount());
    }
    network.promotedStages_ = std::move(promoted);
    if (network.promotedStages_.size() < network.StageCount()) {
        network.promotedStages_.resize(network.StageCount(), false);
    }
    if (in.bad()) {
        throw std::runtime_error("failed to read n-tuple weights: " + path);
    }
    return network;
}

std::size_t NtupleNetwork::EntriesForPattern(const NtuplePattern& pattern) {
    std::size_t entries = 1;
    for (std::size_t index = 0; index < pattern.cells.size(); ++index) {
        entries *= 16U;
    }
    return entries;
}

std::size_t NtupleNetwork::KeyFor(const FastBoard& board, const NtuplePattern& pattern) {
    std::size_t key = 0;
    std::size_t shift = 0;
    for (int cell : pattern.cells) {
        const int clamped = std::clamp(cell, 0, kCellCount - 1);
        key |= static_cast<std::size_t>(board.GetRank(clamped) & 0xF) << shift;
        shift += 4U;
    }
    return key;
}

void NtupleNetwork::BuildFixedPathCache() {
    fixedShifts6_.clear();
    fixedOffsets_.clear();
    if (!patternSet_.useFixedPath) {
        return;
    }
    if (!patternSet_.useD4) {
        patternSet_.useFixedPath = false;
        return;
    }
    for (const auto& pattern : patterns_) {
        if (pattern.cells.size() != 6U) {
            patternSet_.useFixedPath = false;
            fixedShifts6_.clear();
            fixedOffsets_.clear();
            return;
        }
    }

    fixedOffsets_ = patternOffsets_;
    fixedShifts6_.resize(patterns_.size() * ntuple_kernel::kFixed6Transforms * ntuple_kernel::kFixed6Cells);
    for (std::size_t patternIndex = 0; patternIndex < patterns_.size(); ++patternIndex) {
        for (std::size_t transform = 0; transform < ntuple_kernel::kFixed6Transforms; ++transform) {
            for (std::size_t cellIndex = 0; cellIndex < ntuple_kernel::kFixed6Cells; ++cellIndex) {
                const int cell = std::clamp(patterns_[patternIndex].cells[cellIndex], 0, kCellCount - 1);
                const int sourceCell = InverseTransformCell(cell, static_cast<int>(transform));
                const std::size_t index = patternIndex * ntuple_kernel::kFixed6Transforms * ntuple_kernel::kFixed6Cells +
                                          cellIndex * ntuple_kernel::kFixed6Transforms + transform;
                fixedShifts6_[index] = static_cast<std::uint8_t>(sourceCell * 4);
            }
        }
    }
}

double NtupleNetwork::EvaluateFixedPath(std::size_t stage, const FastBoard& board) const {
    const auto& weights = stageValues_[stage];
    return ntuple_kernel::EvaluateFixed6(board.Bits(), weights.data(), fixedOffsets_.data(), fixedShifts6_.data(),
                                         fixedOffsets_.size());
}

void NtupleNetwork::ApplyCollectedWeightDeltas(std::size_t stage, const std::size_t* keys,
                                               std::size_t keyCount, float delta, LearningMode mode) {
    auto& weights = stageValues_[stage];
    if (!UsesTc(mode)) {
        for (std::size_t index = 0; index < keyCount; ++index) {
            const std::size_t key = keys[index];
            weights[key] += delta;
        }
        return;
    }
    EnsureTcStage(stage);
    auto& tcStage = tc_[stage];
    for (std::size_t index = 0; index < keyCount; ++index) {
        const std::size_t key = keys[index];
        auto [entry, inserted] = tcStage.try_emplace(key, TcEntry {kTcInitial, kTcInitial});
        TcEntry& tc = entry->second;
        const float adjustedDelta = delta * (std::abs(tc.accum) / tc.updvu);
        tc.accum += delta;
        tc.updvu += std::abs(delta);
        weights[key] += adjustedDelta;
    }
}

void NtupleNetwork::ApplyWeightDelta(std::size_t stage, std::size_t weightIndex, float delta, LearningMode mode) {
    float adjustedDelta = delta;
    if (UsesTc(mode)) {
        TcEntry& tc = TcAt(stage, weightIndex);
        adjustedDelta = delta * (std::abs(tc.accum) / tc.updvu);
        tc.accum += delta;
        tc.updvu += std::abs(delta);
    }
    stageValues_[stage][weightIndex] += adjustedDelta;
}

std::size_t NtupleNetwork::StageForRead(std::size_t stage) const {
    if (stage >= StageCount()) {
        return 0;
    }
    while (stage > 0 && (stage >= promotedStages_.size() || !promotedStages_[stage])) {
        --stage;
    }
    return stage;
}

void NtupleNetwork::EnsureStage(std::size_t stage) {
    if (stage >= StageCount()) {
        return;
    }
    if (stageValues_.size() < StageCount()) {
        stageValues_.resize(StageCount());
    }
    if (tc_.size() < StageCount()) {
        tc_.resize(StageCount());
    }
    if (promotedStages_.size() < StageCount()) {
        promotedStages_.resize(StageCount(), false);
    }
    if (stageValues_[stage].empty()) {
        stageValues_[stage].assign(entriesPerStage_, 0.0F);
    }
    promotedStages_[stage] = true;
}

void NtupleNetwork::EnsureTcStage(std::size_t stage) {
    EnsureStage(stage);
    if (stage >= tc_.size()) {
        tc_.resize(StageCount());
    }
}

float NtupleNetwork::WeightAt(std::size_t stage, std::size_t index) const {
    stage = std::min(stage, stageValues_.empty() ? std::size_t {0} : stageValues_.size() - 1U);
    if (stage < stageValues_.size() && index < stageValues_[stage].size()) {
        return stageValues_[stage][index];
    }
    return 0.0F;
}

void NtupleNetwork::SetWeight(std::size_t stage, std::size_t index, float value) {
    EnsureStage(stage);
    if (stage < stageValues_.size() && index < stageValues_[stage].size()) {
        stageValues_[stage][index] = value;
    }
}

NtupleNetwork::TcEntry& NtupleNetwork::TcAt(std::size_t stage, std::size_t index) {
    EnsureTcStage(stage);
    auto [entry, inserted] = tc_[stage].try_emplace(index, TcEntry {kTcInitial, kTcInitial});
    return entry->second;
}

NtupleTrainer::NtupleTrainer(NtupleNetwork& network)
    : network_(network) {}

NtupleTrainingRate NtupleTrainingRates(const NtupleTrainingOptions& options, std::size_t gameIndex) {
    if (options.games <= 1) {
        return {options.finalAlpha, options.finalExplorationRate};
    }

    const double progress = static_cast<double>(std::min(gameIndex, options.games - 1)) /
                            static_cast<double>(options.games - 1);
    return {
        options.alpha + (options.finalAlpha - options.alpha) * progress,
        options.explorationRate + (options.finalExplorationRate - options.explorationRate) * progress,
    };
}

double TrainingSelectionValue(const BenchmarkSummary& summary, SelectionMetric metric, int targetTile) {
    if (metric == SelectionMetric::AverageScore) {
        return summary.averageScore;
    }
    const auto found = summary.achievementRates.find(targetTile);
    return found == summary.achievementRates.end() ? 0.0 : found->second;
}

NtupleTrainingStats ApplyBackwardAfterstateTrace(NtupleNetwork& network,
                                                 const std::vector<NtupleTraceStep>& trace,
                                                 double alpha,
                                                 LearningMode mode) {
    return ApplyBackwardAfterstateTraceInternal(network, trace, alpha, mode, 0.0);
}

NtupleTrainingStats NtupleTrainer::Train(const NtupleTrainingOptions& options) {
    NtupleTrainingStats stats;
    stats.games = options.games;
    stats.stageUpdates.assign(std::max<std::size_t>(1, network_.StageCount()), 0);
    double absErrorSum = 0.0;
    double squaredErrorSum = 0.0;
    std::vector<NtupleTraceStep> trace;
    trace.reserve(4096);

    for (std::size_t gameIndex = 0; gameIndex < options.games; ++gameIndex) {
        Random rng(options.seed + gameIndex);
        const bool usePrior = options.priorWeight != 0.0;
        Evaluator evaluator;
        const NtupleTrainingRate rates = NtupleTrainingRates(options, gameIndex);
        bool usedReplay = false;
        FastBoard board = InitialBoardForGame(options, rng, usedReplay);
        if (usedReplay) {
            ++stats.replayStarts;
        }

        std::size_t movesThisGame = 0;
        bool capturedReplayThisGame = false;
        auto captureReplay = [&]() {
            if (options.replayCaptureRank > 0 && !capturedReplayThisGame &&
                board.MaxRank() >= options.replayCaptureRank) {
                replayStarts_.push_back(board);
                capturedReplayThisGame = true;
                ++stats.replayCaptured;
            }
        };
        captureReplay();

        if (options.updateOrder == NtupleUpdateOrder::Backward) {
            trace.clear();
            while (board.CanMove() &&
                   (options.maxMovesPerGame == 0 || movesThisGame < options.maxMovesPerGame)) {
                const CandidateMove candidate = ChooseMove(board, rng, rates.explorationRate, options.priorWeight);
                if (!candidate.valid) {
                    break;
                }

                trace.emplace_back(candidate.move.board, candidate.move.scoreDelta);
                board = FastBoard(candidate.move.board);
                rng.SpawnOnFastBoard(board);
                captureReplay();
                ++movesThisGame;
            }
            const NtupleTrainingStats traceStats =
                ApplyBackwardAfterstateTraceInternal(network_, trace, rates.alpha, options.learningMode,
                                                     options.priorWeight);
            stats.moves += traceStats.moves;
            stats.updates += traceStats.updates;
            stats.totalScore += traceStats.totalScore;
            stats.maxAbsTdError = std::max(stats.maxAbsTdError, traceStats.maxAbsTdError);
            absErrorSum += traceStats.meanAbsTdError * static_cast<double>(traceStats.updates);
            squaredErrorSum += traceStats.rmsTdError * traceStats.rmsTdError *
                               static_cast<double>(traceStats.updates);
            if (stats.stageUpdates.size() < traceStats.stageUpdates.size()) {
                stats.stageUpdates.resize(traceStats.stageUpdates.size(), 0);
            }
            for (std::size_t index = 0; index < traceStats.stageUpdates.size(); ++index) {
                stats.stageUpdates[index] += traceStats.stageUpdates[index];
            }
        } else {
            while (board.CanMove() &&
                   (options.maxMovesPerGame == 0 || movesThisGame < options.maxMovesPerGame)) {
                const CandidateMove candidate = ChooseMove(board, rng, rates.explorationRate, options.priorWeight);
                if (!candidate.valid) {
                    break;
                }

                const FastBoard afterMove(candidate.move.board);
                const bool useExpected = options.useExpectedSpawnTarget &&
                                         (options.expectedTargetEmptyThreshold <= 0 ||
                                          afterMove.CountEmpty() <= options.expectedTargetEmptyThreshold);
                const double nextValue = useExpected
                                             ? ExpectedSpawnTarget(afterMove, network_, evaluator, options.priorWeight)
                                             : 0.0;
                board = afterMove;
                rng.SpawnOnFastBoard(board);
                captureReplay();

                const double sampledNextValue = useExpected
                                                    ? nextValue
                                                    : BestMoveValueAfterSpawn(board, network_, evaluator,
                                                                              options.priorWeight);
                const double target = options.discount * sampledNextValue;
                const double residualTarget = usePrior
                                                  ? target - options.priorWeight * evaluator.Evaluate(afterMove)
                                                  : target;
                const std::size_t updateStage = network_.StageFor(afterMove);
                if (updateStage >= stats.stageUpdates.size()) {
                    stats.stageUpdates.resize(updateStage + 1U, 0);
                }
                const NtupleUpdateStats updateStats =
                    network_.UpdateTowardFast(afterMove, residualTarget, rates.alpha, options.learningMode);
                const double absError = std::abs(updateStats.error);
                absErrorSum += absError;
                squaredErrorSum += updateStats.error * updateStats.error;
                stats.maxAbsTdError = std::max(stats.maxAbsTdError, absError);
                ++stats.stageUpdates[updateStage];

                ++movesThisGame;
                ++stats.moves;
                ++stats.updates;
                stats.totalScore += candidate.move.scoreDelta;
            }
        }

        stats.maxTile = std::max(stats.maxTile, board.MaxTile());
    }
    if (stats.updates > 0) {
        stats.meanAbsTdError = absErrorSum / static_cast<double>(stats.updates);
        stats.rmsTdError = std::sqrt(squaredErrorSum / static_cast<double>(stats.updates));
    }
    stats.tcTouchedEntries = network_.TouchedTcEntries();

    return stats;
}

void NtupleTrainer::AddReplayStart(const FastBoard& board) {
    replayStarts_.push_back(board);
}

std::size_t NtupleTrainer::ReplayStartCount() const {
    return replayStarts_.size();
}

FastBoard NtupleTrainer::InitialBoardForGame(const NtupleTrainingOptions& options, Random& rng,
                                             bool& usedReplay) const {
    usedReplay = false;
    if (options.replayStartRank > 0 && !replayStarts_.empty()) {
        std::size_t eligible = 0;
        for (const FastBoard& board : replayStarts_) {
            if (board.MaxRank() >= options.replayStartRank) {
                ++eligible;
            }
        }
        if (eligible > 0) {
            const std::size_t selected = rng.NextIndex(eligible);
            std::size_t seen = 0;
            for (const FastBoard& board : replayStarts_) {
                if (board.MaxRank() < options.replayStartRank) {
                    continue;
                }
                if (seen == selected) {
                    usedReplay = true;
                    return board;
                }
                ++seen;
            }
        }
    }
    return MakeStageStartBoard(options.startRank, rng);
}

NtupleTrainer::CandidateMove NtupleTrainer::ChooseMove(const FastBoard& board, Random& rng,
                                                       double explorationRate, double priorWeight) const {
    std::array<CandidateMove, 4> legalMoves {};
    std::size_t legalCount = 0;
    for (Direction direction : std::array<Direction, 4> {
             Direction::Left,
             Direction::Up,
             Direction::Right,
             Direction::Down,
         }) {
        const FastMoveResult move = ApplyDirection(board, direction);
        if (move.changed) {
            legalMoves[legalCount] = {move, true};
            ++legalCount;
        }
    }

    if (legalCount == 0) {
        return {};
    }
    if (explorationRate > 0.0 && rng.NextUnit() < explorationRate) {
        return legalMoves[rng.NextIndex(legalCount)];
    }

    double bestValue = -std::numeric_limits<double>::infinity();
    std::size_t bestIndex = 0;
    std::optional<Evaluator> evaluator;
    if (priorWeight != 0.0) {
        evaluator.emplace();
    }
    for (std::size_t index = 0; index < legalCount; ++index) {
        const FastBoard afterMove(legalMoves[index].move.board);
        double value = static_cast<double>(legalMoves[index].move.scoreDelta) + network_.Evaluate(afterMove);
        if (priorWeight != 0.0) {
            value += priorWeight * evaluator->Evaluate(afterMove);
        }
        if (index == 0 || value > bestValue) {
            bestValue = value;
            bestIndex = index;
        }
    }
    return legalMoves[bestIndex];
}

NtupleAgent::NtupleAgent() = default;

NtupleAgent::NtupleAgent(NtupleNetwork network)
    : network_(std::move(network)) {}

MoveDecision NtupleAgent::ChooseMove(const FastBoard& board) const {
    return ChooseNtupleMove(board, Network(), evaluator_, priorWeight_);
}

void NtupleAgent::SetNetwork(NtupleNetwork network) {
    network_ = std::move(network);
    networkShared_.reset();
}

void NtupleAgent::SetNetworkShared(std::shared_ptr<const NtupleNetwork> network) {
    networkShared_ = std::move(network);
}

const NtupleNetwork& NtupleAgent::Network() const {
    return networkShared_ ? *networkShared_ : network_;
}

void NtupleAgent::SetPriorWeight(double weight) {
    priorWeight_ = weight;
}

double NtupleAgent::PriorWeight() const {
    return priorWeight_;
}

}  // namespace game2048::ai
