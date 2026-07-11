#pragma once

namespace vm::api {
	enum class ProcessMode { Safe, Fast };

	struct ProcessConfig final {
		ProcessMode mode                      = ProcessMode::Safe;
		bool        enable_deadlock_detection = false;
		bool        enable_fast_track         = false;
	};
}
