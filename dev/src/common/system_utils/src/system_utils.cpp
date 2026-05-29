#include "system_utils.hpp"

#include <cerrno>

#ifdef _WIN32
	#include <process.h>
#else
	#include <unistd.h>
#endif

// @TODO: #2308 Once implemented, maybe use it here.
ExecSelfResult execSelf(std::vector<std::string>& g_argv) {
	std::vector<char*> args;
	args.reserve(g_argv.size() + 1);
	for (auto& arg: g_argv) args.push_back(arg.data());
	args.push_back(nullptr);

#ifdef _WIN32
	intptr_t result = _spawnvp(_P_WAIT, args[0], args.data());
	if (result == -1) {
		std::perror("_spawnvp");
		return ExecSelfResult{
			.status     = ExecSelfStatus::Error,
			.error_code = errno,
		};
	}
	return ExecSelfResult{
		.status    = ExecSelfStatus::Spawned,
		.exit_code = static_cast<int>(result),
	};
#else
	execvp(args[0], args.data());
	std::perror("execvp");
	return ExecSelfResult{
		.status     = ExecSelfStatus::Error,
		.error_code = errno,
	};
#endif
}
