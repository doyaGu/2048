#include "../src/cli/cli_app.h"
#include "test_framework.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

TEST_CASE(CliRun_InvalidInspectBoard_ReturnsNonZeroInsteadOfAborting) {
    char arg0[] = "game2048";
    char arg1[] = "inspect";
    char arg2[] = "--board";
    char arg3[] = "bad";
    char* argv[] = {arg0, arg1, arg2, arg3};

    std::ostringstream capturedError;
    std::streambuf* originalError = std::cerr.rdbuf(capturedError.rdbuf());
    const int code = game2048::RunCli(4, argv);
    std::cerr.rdbuf(originalError);

    EXPECT_EQ(code, 1);
    EXPECT_TRUE(capturedError.str().find("invalid --board cell at row 1, column 1") != std::string::npos);
    EXPECT_TRUE(capturedError.str().find("stoi") == std::string::npos);
}

TEST_CASE(CliRun_InspectProfile_UsesStrongSearchBranchLimit) {
    const std::filesystem::path profilePath = "/tmp/game2048_inspect_strong_search.toml";
    {
        std::ofstream profile(profilePath);
        profile << R"(
[run]
name = "inspect-strong-search"

[search]
agent = "expectimax"
depth = 1
fixed_ply = true
approximate_chance_nodes = false
max_chance_branches_per_value = 1

[[phases]]
name = "noop"
games = 1
)";
    }

    char arg0[] = "game2048";
    char arg1[] = "inspect";
    char arg2[] = "--profile";
    char arg3[] = "/tmp/game2048_inspect_strong_search.toml";
    char arg4[] = "--board";
    char arg5[] = "2,0,0,0/0,0,0,0/0,0,0,0/0,0,0,2";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

    std::ostringstream capturedOutput;
    std::streambuf* originalOutput = std::cout.rdbuf(capturedOutput.rdbuf());
    const int code = game2048::RunCli(6, argv);
    std::cout.rdbuf(originalOutput);
    std::filesystem::remove(profilePath);

    EXPECT_EQ(code, 0);
    EXPECT_TRUE(capturedOutput.str().find("nodes=116") != std::string::npos);
}
