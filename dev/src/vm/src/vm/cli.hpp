/**
 * @file cli.hpp
 * @brief Command line interface for the VM.
 */

#pragma once

#include <vm/core/supervisor/supervisor.hpp>

#include <vm/api/settings.hpp>

int cli(const fs::File& filepath, const std::vector<std::string>& args = {}, const vm::api::ProcessSettings& settings = {});
int cli();
