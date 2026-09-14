#include "exec_self.hpp"

#include <base/config/target_info.hpp>

#include <cerrno>

#if BASE_TARGET_OS_WINDOWS
	#include <process.h>
#else
	#include <unistd.h>
#endif

namespace os_utils {

	ExecSelfResult execSelf(std::vector<std::string>& g_argv) {
		std::vector<char*> args;
		args.reserve(g_argv.size() + 1);
		for (auto& arg: g_argv) args.push_back(arg.data());
		args.push_back(nullptr);

#if BASE_TARGET_OS_WINDOWS
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

}
