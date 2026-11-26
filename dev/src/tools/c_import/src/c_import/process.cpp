// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "process.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <format>
#include <fstream>
#include <sstream>

extern char** environ;

namespace c_import {

	namespace {

		std::string readAll(const std::filesystem::path& path) {
			std::ifstream     in(path, std::ios::binary);
			std::stringstream text;
			text << in.rdbuf();
			return text.str();
		}

	}

	std::expected<ProcessOutput, std::string> runProcess(
		const std::string&              program,
		const std::vector<std::string>& args,
		const std::filesystem::path&    cwd
	) {
		// Output goes to files rather than pipes, so a chatty program cannot block on a full pipe.
		TempDir    capture;
		const auto out_path = (capture.path() / "out").string();
		const auto err_path = (capture.path() / "err").string();

		std::vector<std::string> argv_strings{ program };
		argv_strings.insert(argv_strings.end(), args.begin(), args.end());
		std::vector<char*> argv;
		argv.reserve(argv_strings.size() + 1);
		for (auto& arg: argv_strings) argv.push_back(arg.data());
		argv.push_back(nullptr);

		posix_spawn_file_actions_t actions;
		posix_spawn_file_actions_init(&actions);
		posix_spawn_file_actions_addopen(
			&actions, STDOUT_FILENO, out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600
		);
		posix_spawn_file_actions_addopen(
			&actions, STDERR_FILENO, err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600
		);
		const std::string cwd_string = cwd.string();
		if (!cwd_string.empty()) posix_spawn_file_actions_addchdir_np(&actions, cwd_string.c_str());

		pid_t pid   = 0;
		int spawned = posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
		posix_spawn_file_actions_destroy(&actions);
		if (spawned != 0)
			return std::unexpected(std::format(
				"cannot run `{}`: {}",
				program,
				std::error_code(spawned, std::generic_category()).message()
			));

		int status = 0;
		while (waitpid(pid, &status, 0) < 0)
			if (errno != EINTR)
				return std::unexpected(std::format("cannot wait for `{}`", program));
		ProcessOutput output;
		output.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
		output.out       = readAll(out_path);
		output.err       = readAll(err_path);
		return output;
	}

	TempDir::TempDir() {
		auto pattern = (std::filesystem::temp_directory_path() / "duckbind-XXXXXX").string();
		if (mkdtemp(pattern.data()) == nullptr)
			throw std::filesystem::filesystem_error(
				"cannot create a temporary directory",
				std::error_code(errno, std::generic_category())
			);
		dir = pattern;
	}

	TempDir::~TempDir() {
		std::error_code ec;
		std::filesystem::remove_all(dir, ec);
	}

}
