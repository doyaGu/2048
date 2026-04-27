#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "search/greedy.h"
#include "core/board_fast.h"
#include "core/rng.h"
#include "shared/stats.h"

namespace game2048::ai {

enum class NtuplePreset {
    CompactD4,
    Tdl4x6Khyeh,
    Tdl8x6KMatsuzaki,
    Compact = CompactD4,
};

enum class LearningMode {
    TD,
    TC,
    OptimisticTD,
    OptimisticTC,
};

enum class NtupleUpdateOrder {
    Online,
    Backward,
};

enum class SelectionMetric {
    AverageScore,
    TileRate,
};

struct NtuplePattern {
    std::vector<int> cells {};
};

struct NtuplePatternSet {
    std::string name = "compact-d4";
    std::vector<NtuplePattern> basePatterns {};
    bool useD4 = true;
    bool useFixedPath = false;
    int entryRadix = 16;
};

struct NtupleUpdateStats {
    double before = 0.0;
    double after = 0.0;
    double error = 0.0;
};

class NtupleNetwork {
public:
    NtupleNetwork();
    explicit NtupleNetwork(std::vector<NtuplePattern> patterns,
                           float optimisticInit = 0.0F,
                           std::vector<int> stageBoundaries = {});
    explicit NtupleNetwork(NtuplePatternSet patternSet,
                           float optimisticInit = 0.0F,
                           std::vector<int> stageBoundaries = {});

    double Evaluate(const FastBoard& board) const;
    std::vector<std::size_t> FeatureKeysForBoard(const FastBoard& board) const;
    NtupleUpdateStats UpdateToward(const FastBoard& board, double target, double alpha);
    NtupleUpdateStats UpdateToward(const FastBoard& board, double target, double alpha, LearningMode mode);
    NtupleUpdateStats UpdateTowardFast(const FastBoard& board, double target, double alpha, LearningMode mode);
    void Save(const std::string& path) const;

    const std::vector<NtuplePattern>& Patterns() const;
    std::size_t WeightCount() const;
    std::size_t StageCount() const;
    std::size_t StageFor(const FastBoard& board) const;
    void PromoteStageFromPrevious(std::size_t stage);
    void EnableStages(std::vector<int> stageBoundaries);
    int StageUpperRank(std::size_t stage) const;
    std::size_t TouchedTcEntries() const;
    bool HasTcState() const;
    const std::string& ProfileMetadata() const;
    void SetProfileMetadata(std::string metadata);
    const NtuplePatternSet& PatternSet() const;

    static NtupleNetwork Load(const std::string& path);

private:
    static std::size_t EntriesForPattern(const NtuplePattern& pattern);
    static std::size_t KeyFor(const FastBoard& board, const NtuplePattern& pattern);
    std::size_t StageForRead(std::size_t stage) const;
    void BuildFixedPathCache();
    double EvaluateFixedPath(std::size_t stage, const FastBoard& board) const;
    void ApplyCollectedWeightDeltas(std::size_t stage, const std::size_t* keys,
                                    std::size_t keyCount, double delta, LearningMode mode);
    void ApplyWeightDelta(std::size_t stage, std::size_t weightIndex, double delta, LearningMode mode);
    void EnsureStage(std::size_t stage);
    void EnsureTcStage(std::size_t stage);
    float WeightAt(std::size_t stage, std::size_t index) const;
    void SetWeight(std::size_t stage, std::size_t index, float value);
    struct TcEntry {
        float accum = 0.0F;
        float updvu = 0.0F;
    };
    using TcStage = std::unordered_map<std::size_t, TcEntry>;
    TcEntry& TcAt(std::size_t stage, std::size_t index);

    NtuplePatternSet patternSet_;
    std::vector<NtuplePattern> patterns_;
    std::vector<std::size_t> patternOffsets_;
    std::vector<int> stageBoundaries_;
    std::vector<std::vector<float>> stageValues_;
    std::vector<TcStage> tc_;
    std::vector<bool> promotedStages_;
    std::vector<std::uint8_t> fixedShifts6_;
    std::vector<std::size_t> fixedOffsets_;
    std::string profileMetadata_;
    std::size_t entriesPerStage_ = 0;
};

struct NtupleTrainingOptions {
    std::size_t games = 100;
    std::uint64_t seed = kDefaultSeed;
    double alpha = 0.01;
    double finalAlpha = 0.01;
    double discount = 1.0;
    double explorationRate = 0.10;
    double finalExplorationRate = 0.10;
    double priorWeight = 0.0;
    bool useExpectedSpawnTarget = false;
    std::size_t maxMovesPerGame = 0;
    std::size_t evalGames = 0;
    std::size_t evalInterval = 0;
    NtuplePreset tuplePreset = NtuplePreset::Compact;
    LearningMode learningMode = LearningMode::TD;
    double optimisticInit = 0.0;
    bool useMultistage = false;
    std::vector<int> stageBoundaries {};
    int expectedTargetEmptyThreshold = 0;
    int startRank = 0;
    int replayStartRank = 0;
    int replayCaptureRank = 0;
    NtupleUpdateOrder updateOrder = NtupleUpdateOrder::Online;
    SelectionMetric selectionMetric = SelectionMetric::AverageScore;
    int selectionTargetTile = 0;
};

struct NtupleTrainingRate {
    double alpha = 0.0;
    double explorationRate = 0.0;
};

struct NtupleTrainingStats {
    std::size_t games = 0;
    std::size_t moves = 0;
    std::size_t updates = 0;
    std::uint64_t totalScore = 0;
    int maxTile = 0;
    double meanAbsTdError = 0.0;
    double rmsTdError = 0.0;
    double maxAbsTdError = 0.0;
    std::size_t tcTouchedEntries = 0;
    std::size_t replayStarts = 0;
    std::size_t replayCaptured = 0;
    std::vector<std::size_t> stageUpdates {};
};

struct NtupleTraceStep {
    NtupleTraceStep() = default;
    NtupleTraceStep(const FastBoard& board, std::uint32_t rewardDelta)
        : afterstateBits(board.Bits()), reward(rewardDelta) {}
    NtupleTraceStep(std::uint64_t boardBits, std::uint32_t rewardDelta)
        : afterstateBits(boardBits), reward(rewardDelta) {}

    FastBoard Afterstate() const { return FastBoard(afterstateBits); }

    std::uint64_t afterstateBits = 0;
    std::uint32_t reward = 0;
};

NtupleTrainingRate NtupleTrainingRates(const NtupleTrainingOptions& options, std::size_t gameIndex);
double TrainingSelectionValue(const BenchmarkSummary& summary, SelectionMetric metric, int targetTile);
NtupleTrainingStats ApplyBackwardAfterstateTrace(NtupleNetwork& network,
                                                 const std::vector<NtupleTraceStep>& trace,
                                                 double alpha,
                                                 LearningMode mode);
NtuplePatternSet PatternSetForPreset(NtuplePreset preset);
std::vector<NtuplePattern> PatternsForPreset(NtuplePreset preset);
std::vector<int> DefaultStageBoundaries();

class NtupleTrainer {
public:
    explicit NtupleTrainer(NtupleNetwork& network);

    NtupleTrainingStats Train(const NtupleTrainingOptions& options);
    void AddReplayStart(const FastBoard& board);
    std::size_t ReplayStartCount() const;

private:
    struct CandidateMove {
        FastMoveResult move {};
        bool valid = false;
    };

    CandidateMove ChooseMove(const FastBoard& board, Random& rng, double explorationRate, double priorWeight) const;
    FastBoard InitialBoardForGame(const NtupleTrainingOptions& options, Random& rng, bool& usedReplay) const;

    NtupleNetwork& network_;
    std::vector<FastBoard> replayStarts_;
};

class NtupleAgent {
public:
    NtupleAgent();
    explicit NtupleAgent(NtupleNetwork network);

    MoveDecision ChooseMove(const FastBoard& board) const;
    void SetNetwork(NtupleNetwork network);
    void SetNetworkShared(std::shared_ptr<const NtupleNetwork> network);
    const NtupleNetwork& Network() const;
    void SetPriorWeight(double weight);
    double PriorWeight() const;

private:
    NtupleNetwork network_;
    std::shared_ptr<const NtupleNetwork> networkShared_;
    Evaluator evaluator_;
    double priorWeight_ = 0.0;
};

std::vector<NtuplePattern> DefaultNtuplePatterns();

}  // namespace game2048::ai
