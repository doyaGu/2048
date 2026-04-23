#include <stdexcept>

#include "../src/cli_options.h"
#include "test_framework.h"

namespace {

using game2048::ParseCliOptions;

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
