#pragma once

namespace vm::api {
	enum class ProcessMode { Safe, Fast };

	struct ProcessConfig {
		ProcessMode mode                      = ProcessMode::Safe;
		bool        enable_deadlock_detection = false;
	};
}
