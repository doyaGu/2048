#include "../src/cli/cli_app.h"
#include "test_framework.h"

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
