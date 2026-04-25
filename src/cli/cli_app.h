#pragma once

#include "cli/cli_options.h"

namespace game2048 {

int RunCli(int argc, char** argv);
int RunCliCommand(const CliOptions& options);

}  // namespace game2048
