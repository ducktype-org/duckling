// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "system_command.hpp"

#include <sys/wait.h>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <logger/logger.hpp>
#include <os_utils/thread_safe_wrappers.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

using std::literals::operator""sv;

namespace system_command {

	SystemCommand& SystemCommand::addArg(std::string arg) {
		arguments.push_back(std::move(arg));
		return *this;
	}

	SystemCommand& SystemCommand::addEnv(std::string name, std::string value) {
		environment.emplace_back(std::move(name), std::move(value));
		return *this;
	}

	std::string SystemCommand::escapedDisplay() const {
		std::string command_display{};
		for (const auto& env: this->environment)
			command_display += '"' + env.first + "=" + env.second + "\" ";
		command_display += '"' + this->program_name + '"';
		for (const auto& arg: this->arguments) command_display += " \"" + arg + '"';
		return command_display;
	}

	std::vector<std::string> SystemCommand::createArgv() const {
		std::vector<std::string> result{};
		result.reserve(this->arguments.size() + 1);
		result.push_back(this->program_name);
		for (const auto& arg: this->arguments) result.push_back(arg);
		return result;
	}

	std::vector<std::string> SystemCommand::createEnvpWithEnviron() const {
		std::vector<std::string> environment = SystemCommand::osEnviron();
		for (const auto& env: this->environment)
			environment.push_back(env.first + "=" + env.second);
		return environment;
	}

	std::vector<const char*> SystemCommand::convertToCStyle(
		const std::vector<std::string>& input, SystemCommand::AppendNullptr append_nullptr
	) {
		std::vector<const char*> result{};
		std::size_t              total_size = input.size();
		if (append_nullptr == SystemCommand::AppendNullptr::Yes) total_size += 1;
		result.reserve(total_size);
		for (const auto& part: input) result.push_back(part.c_str());
		if (append_nullptr == AppendNullptr::Yes) result.push_back(nullptr);
		return result;
	}

	std::vector<std::string> SystemCommand::osEnviron() {
		char**                   ptr = SystemCommand::getOsEnvironPointer();
		std::vector<std::string> result{};
		for (std::size_t i = 0; ptr[i] != nullptr; i++) result.emplace_back(ptr[i]);
		return result;
	}
}
