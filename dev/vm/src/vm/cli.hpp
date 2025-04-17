/**
 * @file cli.hpp
 * @brief Command line interface for the VM.
 */

#pragma once

#include <vm/core/supervisor/supervisor.hpp>

void cli(const fs::FilePath& filepath, const std::vector<std::string>& args);
void cli();
