/**
 * @file cli.hpp
 * @brief Command line interface for the VM.
 */

#pragma once

#include <vm/core/supervisor/supervisor.hpp>

void cli(const fs::FilePath& filepath, bool load_stdlib);
void cli(bool load_stdlib);
