#pragma once

namespace vm::api {
	enum class ProcessMode { Safe, Fast };

	struct ProcessConfig {
		ProcessMode mode = ProcessMode::Safe;
	};
}
