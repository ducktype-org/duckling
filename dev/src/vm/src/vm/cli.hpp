/**
 * @file cli.hpp
 * @brief Command line interface for the VM.
 */

#pragma once

#include <vm/core/supervisor/supervisor.hpp>

int cli(const fs::File& filepath);
int cli();
