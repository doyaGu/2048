#include "experiment/profile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace game2048::experiment {

namespace {

struct TomlValue {
    enum class Kind {
        String,
        Number,
        Bool,
        Array,
    };

    Kind kind = Kind::String;
    std::string scalar {};
    std::vector<TomlValue> array {};
};

struct TomlDocument {
    std::unordered_map<std::string, std::unordered_map<std::string, TomlValue>> tables;
    std::vector<std::unordered_map<std::string, TomlValue>> phases;
};

std::string Trim(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
        --last;
    }
    return value.substr(first, last - first);
}

std::string StripComment(const std::string& line) {
    bool inString = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (ch == '"') {
            inString = !inString;
        }
        if (ch == '#' && !inString) {
            return line.substr(0, index);
        }
    }
    return line;
}

TomlValue ParseValue(const std::string& raw);

std::vector<std::string> SplitArrayItems(const std::string& inner) {
    std::vector<std::string> items;
    bool inString = false;
    std::size_t start = 0;
    for (std::size_t index = 0; index < inner.size(); ++index) {
        const char ch = inner[index];
        if (ch == '"') {
            inString = !inString;
        }
        if (ch == ',' && !inString) {
            items.push_back(Trim(inner.substr(start, index - start)));
            start = index + 1;
        }
    }
    const std::string tail = Trim(inner.substr(start));
    if (!tail.empty()) {
        items.push_back(tail);
    }
    return items;
}

TomlValue ParseValue(const std::string& raw) {
    const std::string value = Trim(raw);
    if (value.empty()) {
        throw std::runtime_error("empty TOML value");
    }
    if (value.front() == '"' && value.back() == '"' && value.size() >= 2) {
        return {TomlValue::Kind::String, value.substr(1, value.size() - 2), {}};
    }
    if (value == "true" || value == "false") {
        return {TomlValue::Kind::Bool, value, {}};
    }
    if (value.front() == '[' && value.back() == ']') {
        TomlValue result;
        result.kind = TomlValue::Kind::Array;
        const std::string inner = Trim(value.substr(1, value.size() - 2));
        if (!inner.empty()) {
            for (const auto& item : SplitArrayItems(inner)) {
                result.array.push_back(ParseValue(item));
            }
        }
        return result;
    }
    return {TomlValue::Kind::Number, value, {}};
}

TomlDocument ParseToml(const std::string& text) {
    TomlDocument doc;
    std::string table;
    bool inPhase = false;
    std::stringstream stream(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        line = Trim(StripComment(line));
        if (line.empty()) {
            continue;
        }
        if (line.rfind("[[", 0) == 0 && line.size() > 4 && line.substr(line.size() - 2) == "]]") {
            table = Trim(line.substr(2, line.size() - 4));
            if (table != "phases") {
                throw std::runtime_error("unsupported TOML array table: " + table);
            }
            doc.phases.emplace_back();
            inPhase = true;
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            table = Trim(line.substr(1, line.size() - 2));
            inPhase = false;
            static_cast<void>(doc.tables[table]);
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("invalid TOML line " + std::to_string(lineNumber));
        }
        const std::string key = Trim(line.substr(0, equals));
        const TomlValue value = ParseValue(line.substr(equals + 1));
        if (key.empty()) {
            throw std::runtime_error("empty TOML key on line " + std::to_string(lineNumber));
        }
        if (inPhase) {
            if (doc.phases.empty()) {
                throw std::runtime_error("phase key before phase table");
            }
            doc.phases.back()[key] = value;
        } else {
            if (table.empty()) {
                throw std::runtime_error("TOML key outside table: " + key);
            }
            doc.tables[table][key] = value;
        }
    }
    return doc;
}

const TomlValue* Find(const std::unordered_map<std::string, TomlValue>& table, const std::string& key) {
    const auto it = table.find(key);
    return it == table.end() ? nullptr : &it->second;
}

std::string ReadString(const std::unordered_map<std::string, TomlValue>& table, const std::string& key,
                       const std::string& fallback) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->kind != TomlValue::Kind::String) {
        throw std::runtime_error("TOML key must be a string: " + key);
    }
    return value->scalar;
}

double ReadDouble(const std::unordered_map<std::string, TomlValue>& table, const std::string& key, double fallback) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->kind != TomlValue::Kind::Number) {
        throw std::runtime_error("TOML key must be a number: " + key);
    }
    return std::stod(value->scalar);
}

std::uint64_t ReadU64(const std::unordered_map<std::string, TomlValue>& table, const std::string& key,
                      std::uint64_t fallback) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->kind != TomlValue::Kind::Number) {
        throw std::runtime_error("TOML key must be an integer: " + key);
    }
    return static_cast<std::uint64_t>(std::stoull(value->scalar));
}

std::size_t ReadSize(const std::unordered_map<std::string, TomlValue>& table, const std::string& key,
                     std::size_t fallback) {
    return static_cast<std::size_t>(ReadU64(table, key, fallback));
}

int ReadInt(const std::unordered_map<std::string, TomlValue>& table, const std::string& key, int fallback) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->kind != TomlValue::Kind::Number) {
        throw std::runtime_error("TOML key must be an integer: " + key);
    }
    return std::stoi(value->scalar);
}

bool ReadBool(const std::unordered_map<std::string, TomlValue>& table, const std::string& key, bool fallback) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->kind != TomlValue::Kind::Bool) {
        throw std::runtime_error("TOML key must be a boolean: " + key);
    }
    return value->scalar == "true";
}

std::vector<int> ReadTileRankArray(const std::unordered_map<std::string, TomlValue>& table, const std::string& key) {
    std::vector<int> ranks;
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return ranks;
    }
    if (value->kind != TomlValue::Kind::Array) {
        throw std::runtime_error("TOML key must be an array: " + key);
    }
    for (const auto& item : value->array) {
        if (item.kind != TomlValue::Kind::Number) {
            throw std::runtime_error("stage boundary array must contain numbers");
        }
        const int boundaryValue = std::stoi(item.scalar);
        if (boundaryValue >= 0 && boundaryValue <= 31) {
            ranks.push_back(boundaryValue);
            continue;
        }
        const int tile = boundaryValue;
        if (tile < 2 || (tile & (tile - 1)) != 0) {
            throw std::runtime_error("stage boundary must be rank or power-of-two tile");
        }
        int rank = 0;
        for (int current = tile; current > 1; current >>= 1) {
            ++rank;
        }
        ranks.push_back(rank);
    }
    return ranks;
}

std::vector<std::uint64_t> ReadU64Array(const std::unordered_map<std::string, TomlValue>& table,
                                        const std::string& key) {
    std::vector<std::uint64_t> values;
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return values;
    }
    if (value->kind != TomlValue::Kind::Array) {
        throw std::runtime_error("TOML key must be an array: " + key);
    }
    for (const auto& item : value->array) {
        if (item.kind != TomlValue::Kind::Number) {
            throw std::runtime_error("array must contain numbers: " + key);
        }
        values.push_back(static_cast<std::uint64_t>(std::stoull(item.scalar)));
    }
    return values;
}

std::vector<std::size_t> ReadSizeArray(const std::unordered_map<std::string, TomlValue>& table,
                                       const std::string& key) {
    std::vector<std::size_t> values;
    for (std::uint64_t value : ReadU64Array(table, key)) {
        values.push_back(static_cast<std::size_t>(value));
    }
    return values;
}

std::vector<double> ReadDoubleArray(const std::unordered_map<std::string, TomlValue>& table,
                                    const std::string& key) {
    std::vector<double> values;
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        return values;
    }
    if (value->kind != TomlValue::Kind::Array) {
        throw std::runtime_error("TOML key must be an array: " + key);
    }
    for (const auto& item : value->array) {
        if (item.kind != TomlValue::Kind::Number) {
            throw std::runtime_error("array must contain numbers: " + key);
        }
        values.push_back(std::stod(item.scalar));
    }
    return values;
}

const std::unordered_map<std::string, TomlValue>& Table(const TomlDocument& doc, std::string_view name) {
    static const std::unordered_map<std::string, TomlValue> empty;
    const auto it = doc.tables.find(std::string(name));
    return it == doc.tables.end() ? empty : it->second;
}

ai::AgentKind ParseAgent(const std::string& value) {
    if (value == "greedy") {
        return ai::AgentKind::Greedy;
    }
    if (value == "expectimax") {
        return ai::AgentKind::Expectimax;
    }
    if (value == "ntuple") {
        return ai::AgentKind::Ntuple;
    }
    throw std::runtime_error("unknown agent: " + value);
}

ai::NtuplePreset ParsePreset(const std::string& value) {
    if (value == "compact-d4") {
        return ai::NtuplePreset::CompactD4;
    }
    if (value == "tdl-4x6-khyeh") {
        return ai::NtuplePreset::Tdl4x6Khyeh;
    }
    if (value == "tdl-8x6-kmatsuzaki") {
        return ai::NtuplePreset::Tdl8x6KMatsuzaki;
    }
    throw std::runtime_error("unknown tuple preset: " + value);
}

ai::LearningMode ParseLearningMode(const std::string& value) {
    if (value == "td") {
        return ai::LearningMode::TD;
    }
    if (value == "tc") {
        return ai::LearningMode::TC;
    }
    if (value == "optimistic-td") {
        return ai::LearningMode::OptimisticTD;
    }
    if (value == "optimistic-tc") {
        return ai::LearningMode::OptimisticTC;
    }
    throw std::runtime_error("unknown learning mode: " + value);
}

ai::NtupleUpdateOrder ParseUpdateOrder(const std::string& value) {
    if (value == "online") {
        return ai::NtupleUpdateOrder::Online;
    }
    if (value == "backward") {
        return ai::NtupleUpdateOrder::Backward;
    }
    throw std::runtime_error("unknown update order: " + value);
}

training::SelectionMetric ParseSelectionMetric(const std::string& value) {
    if (value == "average-score") {
        return training::SelectionMetric::AverageScore;
    }
    if (value == "tile-rate") {
        return training::SelectionMetric::TileRate;
    }
    throw std::runtime_error("unknown selection metric: " + value);
}

std::string PriorTag(double prior) {
    std::ostringstream out;
    out << prior;
    std::string tag = out.str();
    std::replace(tag.begin(), tag.end(), '.', 'p');
    return tag;
}

SearchProfileConfig::DowngradeMode ParseDowngradeMode(const std::string& value) {
    if (value == "leaf") {
        return SearchProfileConfig::DowngradeMode::Leaf;
    }
    if (value == "root") {
        return SearchProfileConfig::DowngradeMode::Root;
    }
    throw std::runtime_error("unknown search.downgrade.mode: " + value);
}

}  // namespace

ExperimentProfile ParseExperimentProfileText(const std::string& text) {
    const TomlDocument doc = ParseToml(text);
    ExperimentProfile profile;

    const auto& run = Table(doc, "run");
    profile.run.name = ReadString(run, "name", profile.run.name);
    profile.run.seed = ReadU64(run, "seed", profile.run.seed);

    const auto& selection = Table(doc, "selection");
    profile.selection.metric = ParseSelectionMetric(ReadString(selection, "metric", "average-score"));
    profile.selection.targetTile = ReadInt(selection, "target_tile", profile.selection.targetTile);
    profile.selection.confidenceZ = ReadDouble(selection, "confidence_z", profile.selection.confidenceZ);
    profile.selection.confirmGames = ReadSize(selection, "confirm_games", profile.selection.confirmGames);
    if (profile.selection.metric == training::SelectionMetric::TileRate && profile.selection.targetTile <= 0) {
        throw std::runtime_error("selection.target_tile is required for tile-rate selection");
    }

    const auto& search = Table(doc, "search");
    profile.search.agent = ParseAgent(ReadString(search, "agent", "expectimax"));
    profile.search.evalAgent = ParseAgent(ReadString(search, "eval_agent", "expectimax"));
    profile.search.depth = ReadInt(search, "depth", profile.search.depth);
    profile.search.timeBudgetMs = ReadInt(search, "time_budget_ms", profile.search.timeBudgetMs);
    profile.search.evalDepth = ReadInt(search, "eval_depth", profile.search.evalDepth);
    profile.search.evalTimeBudgetMs = ReadInt(search, "eval_time_budget_ms", profile.search.evalTimeBudgetMs);
    profile.search.fixedPly = ReadBool(search, "fixed_ply", profile.search.fixedPly);
    profile.search.approximateChanceNodes = ReadBool(search, "approximate_chance_nodes",
                                                     profile.search.approximateChanceNodes);
    profile.search.maxChanceBranchesPerValue = ReadInt(search, "max_chance_branches_per_value",
                                                       profile.search.maxChanceBranchesPerValue);
    profile.search.preserveChanceProbabilityMass = ReadBool(search, "preserve_chance_probability_mass",
                                                            profile.search.preserveChanceProbabilityMass);
    profile.search.adaptiveEndgameSearch = ReadBool(search, "adaptive_endgame_search",
                                                    profile.search.adaptiveEndgameSearch);
    profile.search.endgameMinRank = ReadInt(search, "endgame_min_rank", profile.search.endgameMinRank);
    profile.search.endgameDepthBonus = ReadInt(search, "endgame_depth_bonus", profile.search.endgameDepthBonus);
    profile.search.endgameMaxChanceBranchesPerValue = ReadInt(search, "endgame_max_chance_branches_per_value",
                                                              profile.search.endgameMaxChanceBranchesPerValue);
    profile.search.endgamePessimism = ReadDouble(search, "endgame_pessimism", profile.search.endgamePessimism);
    profile.search.canonicalizeTranspositionKeys = ReadBool(search, "canonicalize_transposition_keys",
                                                            profile.search.canonicalizeTranspositionKeys);
    profile.search.rootRollout = ReadBool(search, "root_rollout", profile.search.rootRollout);
    profile.search.rootRolloutDepth = ReadInt(search, "root_rollout_depth", profile.search.rootRolloutDepth);
    profile.search.rootRolloutWeight = ReadDouble(search, "root_rollout_weight", profile.search.rootRolloutWeight);
    profile.search.evalGames = ReadSize(search, "eval_games", profile.search.evalGames);
    profile.search.evalInterval = ReadSize(search, "eval_interval", profile.search.evalInterval);
    profile.search.progressIntervalGames = ReadSize(search, "progress_interval_games",
                                                    profile.search.progressIntervalGames);
    profile.search.finalGames = ReadSize(search, "final_games", profile.search.finalGames);
    profile.search.evalThreads = ReadSize(search, "eval_threads", profile.search.evalThreads);
    const auto& downgrade = Table(doc, "search.downgrade");
    profile.search.downgrade.enabled = ReadBool(downgrade, "enabled", profile.search.downgrade.enabled);
    profile.search.downgrade.mode = ParseDowngradeMode(ReadString(downgrade, "mode", "leaf"));
    profile.search.downgrade.steps = ReadInt(downgrade, "steps", profile.search.downgrade.steps);
    profile.search.downgrade.floorRank = ReadInt(downgrade, "floor_rank", profile.search.downgrade.floorRank);

    const auto& value = Table(doc, "value");
    profile.value.tuplePreset = ParsePreset(ReadString(value, "preset", "compact-d4"));
    profile.value.optimisticInit = ReadDouble(value, "optimistic_init", profile.value.optimisticInit);
    profile.value.multistage = ReadBool(value, "multistage", profile.value.multistage);
    profile.value.stageBoundaries = ReadTileRankArray(value, "stage_boundaries");
    const auto& storage = Table(doc, "value.storage");
    profile.value.storageMode = ReadString(storage, "mode", profile.value.storageMode);
    if (profile.value.storageMode != "dense-stage") {
        throw std::runtime_error("unsupported value.storage.mode: " + profile.value.storageMode);
    }

    const auto& trainer = Table(doc, "trainer");
    profile.trainer.mode = ReadString(trainer, "mode", profile.trainer.mode);
    profile.trainer.games = ReadSize(trainer, "games", profile.trainer.games);
    profile.trainer.progressIntervalGames = ReadSize(trainer, "progress_interval_games",
                                                     profile.trainer.progressIntervalGames);
    profile.trainer.alpha = ReadDouble(trainer, "alpha", profile.trainer.alpha);
    profile.trainer.learningMode = ParseLearningMode(ReadString(trainer, "learning_mode", "td"));
    profile.trainer.checkpoints = ReadSizeArray(trainer, "checkpoints");
    profile.trainer.fastPath = ReadBool(trainer, "fast_path", profile.trainer.fastPath);

    const auto& eval = Table(doc, "eval");
    profile.eval.mode = ReadString(eval, "mode", profile.eval.mode);
    profile.eval.games = ReadSize(eval, "games", profile.eval.games);
    profile.eval.parityTolerance = ReadDouble(eval, "parity_tolerance", profile.eval.parityTolerance);
    profile.eval.referenceCachePath = ReadString(eval, "reference_cache", profile.eval.referenceCachePath);

    const auto& artifacts = Table(doc, "artifacts");
    profile.artifacts.dir = ReadString(artifacts, "dir", profile.artifacts.dir);

    const auto& matrix = Table(doc, "matrix");
    profile.matrix.seeds = ReadU64Array(matrix, "seeds");
    profile.matrix.priors = ReadDoubleArray(matrix, "priors");

    for (const auto& phaseTable : doc.phases) {
        TrainingPhaseConfig phase;
        phase.name = ReadString(phaseTable, "name", phase.name);
        phase.games = ReadSize(phaseTable, "games", phase.games);
        phase.learningMode = ParseLearningMode(ReadString(phaseTable, "learning_mode", "td"));
        phase.alpha = ReadDouble(phaseTable, "alpha", phase.alpha);
        phase.finalAlpha = ReadDouble(phaseTable, "final_alpha", phase.alpha);
        phase.epsilon = ReadDouble(phaseTable, "epsilon", phase.epsilon);
        phase.finalEpsilon = ReadDouble(phaseTable, "final_epsilon", phase.epsilon);
        phase.priorWeight = ReadDouble(phaseTable, "prior_weight", phase.priorWeight);
        phase.startRank = ReadInt(phaseTable, "start_rank", phase.startRank);
        phase.replayStartRank = ReadInt(phaseTable, "replay_start_rank", phase.replayStartRank);
        phase.replayCaptureRank = ReadInt(phaseTable, "replay_capture_rank", phase.replayCaptureRank);
        phase.updateOrder = ParseUpdateOrder(ReadString(phaseTable, "update_order", "online"));
        phase.enableMultistage = ReadBool(phaseTable, "enable_multistage", phase.enableMultistage);
        profile.phases.push_back(phase);
    }
    if (profile.phases.empty() && profile.trainer.mode.empty()) {
        throw std::runtime_error("profile must contain at least one [[phases]] entry");
    }
    return profile;
}

ExperimentProfile LoadExperimentProfile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open profile: " + path);
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    return ParseExperimentProfileText(buffer.str());
}

void SaveProfileCopy(const std::string& sourcePath, const std::string& destinationPath) {
    std::ifstream input(sourcePath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open profile: " + sourcePath);
    }
    std::ofstream output(destinationPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not write profile copy: " + destinationPath);
    }
    output << input.rdbuf();
}

std::vector<ExperimentProfile> ExpandMatrixJobs(const ExperimentProfile& profile) {
    const std::vector<std::uint64_t> seeds = profile.matrix.seeds.empty()
        ? std::vector<std::uint64_t> {profile.run.seed}
        : profile.matrix.seeds;
    const std::vector<double> priors = profile.matrix.priors.empty()
        ? std::vector<double> {0.0}
        : profile.matrix.priors;

    std::vector<ExperimentProfile> jobs;
    for (std::uint64_t seed : seeds) {
        for (double prior : priors) {
            ExperimentProfile job = profile;
            job.run.seed = seed;
            job.run.name = profile.run.name + "-seed" + std::to_string(seed) + "-prior" + PriorTag(prior);
            job.artifacts.dir = profile.artifacts.dir + "/" + job.run.name;
            for (auto& phase : job.phases) {
                phase.priorWeight = prior;
            }
            jobs.push_back(job);
        }
    }
    return jobs;
}

}  // namespace game2048::experiment
