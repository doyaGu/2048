#include <stdexcept>

#include "../src/cli/cli_options.h"
#include "test_framework.h"

namespace {

using game2048::ParseCliOptions;

TEST_CASE(CliOptions_DefaultsToPlayCommand) {
    char arg0[] = "game2048";
    char* argv[] = {arg0};

    const auto options = ParseCliOptions(1, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Play);
    EXPECT_FALSE(options.profilePath.has_value());
}

TEST_CASE(CliOptions_PlaySubcommand_ParsesAiAndSearch) {
    char arg0[] = "game2048";
    char arg1[] = "play";
    char arg2[] = "--seed";
    char arg3[] = "42";
    char arg4[] = "--ai";
    char arg5[] = "greedy";
    char arg6[] = "--depth";
    char arg7[] = "2";
    char arg8[] = "--time-budget-ms";
    char arg9[] = "5";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9};

    const auto options = ParseCliOptions(10, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Play);
    EXPECT_EQ(options.seed, 42ULL);
    EXPECT_EQ(options.agent, game2048::ai::AgentKind::Greedy);
    EXPECT_EQ(options.search.maxDepth, 2);
    EXPECT_EQ(options.search.timeBudgetMs, 5);
}

TEST_CASE(CliOptions_PlaySubcommand_AllowsWeights) {
    char arg0[] = "game2048";
    char arg1[] = "play";
    char arg2[] = "--weights";
    char arg3[] = "artifacts/best.weights";
    char* argv[] = {arg0, arg1, arg2, arg3};

    const auto options = ParseCliOptions(4, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Play);
    EXPECT_EQ(*options.weightPath, std::string("artifacts/best.weights"));
}

TEST_CASE(CliOptions_TrainSubcommand_ParsesProfileOnly) {
    char arg0[] = "game2048";
    char arg1[] = "train";
    char arg2[] = "--profile";
    char arg3[] = "profiles/smoke.toml";
    char* argv[] = {arg0, arg1, arg2, arg3};

    const auto options = ParseCliOptions(4, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Train);
    EXPECT_EQ(*options.profilePath, std::string("profiles/smoke.toml"));
}

TEST_CASE(CliOptions_BenchSubcommand_ParsesProfileAndWeights) {
    char arg0[] = "game2048";
    char arg1[] = "bench";
    char arg2[] = "--profile";
    char arg3[] = "profiles/smoke.toml";
    char arg4[] = "--weights";
    char arg5[] = "artifacts/best.weights";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    const auto options = ParseCliOptions(6, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Bench);
    EXPECT_EQ(*options.profilePath, std::string("profiles/smoke.toml"));
    EXPECT_EQ(*options.weightPath, std::string("artifacts/best.weights"));
}

TEST_CASE(CliOptions_MatrixSubcommand_ParsesProfileAndMaxJobs) {
    char arg0[] = "game2048";
    char arg1[] = "matrix";
    char arg2[] = "--profile";
    char arg3[] = "profiles/smoke.toml";
    char arg4[] = "--max-jobs";
    char arg5[] = "2";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    const auto options = ParseCliOptions(6, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Matrix);
    EXPECT_EQ(*options.profilePath, std::string("profiles/smoke.toml"));
    EXPECT_EQ(options.maxJobs, 2);
}

TEST_CASE(CliOptions_MicrobenchSubcommand_ParsesProfileAndGames) {
    char arg0[] = "game2048";
    char arg1[] = "microbench";
    char arg2[] = "--profile";
    char arg3[] = "profiles/ntuple_server.toml";
    char arg4[] = "--games";
    char arg5[] = "200";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    const auto options = ParseCliOptions(6, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Microbench);
    EXPECT_EQ(*options.profilePath, std::string("profiles/ntuple_server.toml"));
    EXPECT_EQ(options.microbenchGames, std::size_t {200});
}

TEST_CASE(CliOptions_ParitySubcommand_ParsesProfileAndTdlBin) {
    char arg0[] = "game2048";
    char arg1[] = "parity";
    char arg2[] = "--profile";
    char arg3[] = "profiles/tdl_parity_smoke.toml";
    char arg4[] = "--tdl-bin";
    char arg5[] = "../TDL2048/2048";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    const auto options = ParseCliOptions(6, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Parity);
    EXPECT_EQ(*options.profilePath, std::string("profiles/tdl_parity_smoke.toml"));
    EXPECT_EQ(*options.tdlBinPath, std::string("../TDL2048/2048"));
}

TEST_CASE(CliOptions_TrainSubcommand_RejectsLegacyLongArguments) {
    char arg0[] = "game2048";
    char arg1[] = "train";
    char arg2[] = "--games";
    char arg3[] = "10";
    char* argv[] = {arg0, arg1, arg2, arg3};

    bool threw = false;
    try {
        static_cast<void>(ParseCliOptions(4, argv));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}

TEST_CASE(CliOptions_InspectSubcommand_ParsesBoardRows) {
    char arg0[] = "game2048";
    char arg1[] = "inspect";
    char arg2[] = "--board";
    char arg3[] = "2,0,0,0/0,4,0,0/0,0,8,0/0,0,0,16";
    char* argv[] = {arg0, arg1, arg2, arg3};

    const auto options = ParseCliOptions(4, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Inspect);
    EXPECT_TRUE(options.boardRows.has_value());
}

TEST_CASE(CliOptions_BenchWithoutProfile_IsRejected) {
    char arg0[] = "game2048";
    char arg1[] = "bench";
    char* argv[] = {arg0, arg1};

    bool threw = false;
    try {
        static_cast<void>(ParseCliOptions(2, argv));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}

TEST_CASE(CliOptions_LegacyBenchmarkSwitch_IsRejected) {
    char arg0[] = "game2048";
    char arg1[] = "--benchmark";
    char arg2[] = "10";
    char* argv[] = {arg0, arg1, arg2};

    bool threw = false;
    try {
        static_cast<void>(ParseCliOptions(3, argv));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}

TEST_CASE(CliOptions_UnknownSwitch_IsRejected) {
    char arg0[] = "game2048";
    char arg1[] = "--bogus";
    char* argv[] = {arg0, arg1};

    bool threw = false;
    try {
        static_cast<void>(ParseCliOptions(2, argv));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}

}  // namespace
