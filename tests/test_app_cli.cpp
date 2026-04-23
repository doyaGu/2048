#include "../src/app/app.h"
#include "test_framework.h"

TEST_CASE(AppRun_InvalidAnalyzeBoard_ReturnsNonZeroInsteadOfAborting) {
    char arg0[] = "game2048";
    char arg1[] = "analyze";
    char arg2[] = "--board";
    char arg3[] = "bad";
    char* argv[] = {arg0, arg1, arg2, arg3};

    game2048::App app;
    const int code = app.Run(4, argv);

    EXPECT_EQ(code, 1);
}

