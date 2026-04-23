#include <stdexcept>

#include "../src/app/cli_options.h"
#include "test_framework.h"

namespace {

using game2048::ParseCliOptions;

TEST_CASE(CliOptions_DefaultsToPlayCommand) {
    char arg0[] = "game2048";
    char* argv[] = {arg0};

    const auto options = ParseCliOptions(1, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Play);
    EXPECT_FALSE(options.benchmarkGames.has_value());
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

TEST_CASE(CliOptions_BenchSubcommand_RequiresGameCount) {
    char arg0[] = "game2048";
    char arg1[] = "bench";
    char arg2[] = "--games";
    char arg3[] = "25";
    char arg4[] = "--csv";
    char arg5[] = "out.csv";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    const auto options = ParseCliOptions(6, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Bench);
    EXPECT_EQ(*options.benchmarkGames, std::size_t {25});
    EXPECT_EQ(*options.csvPath, std::string("out.csv"));
}

TEST_CASE(CliOptions_AnalyzeSubcommand_ParsesBoardRows) {
    char arg0[] = "game2048";
    char arg1[] = "analyze";
    char arg2[] = "--board";
    char arg3[] = "2,0,0,0/0,4,0,0/0,0,8,0/0,0,0,16";
    char* argv[] = {arg0, arg1, arg2, arg3};

    const auto options = ParseCliOptions(4, argv);

    EXPECT_EQ(options.command, game2048::CliCommand::Analyze);
    EXPECT_TRUE(options.boardRows.has_value());
}

TEST_CASE(CliOptions_BenchWithoutGames_IsRejected) {
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
