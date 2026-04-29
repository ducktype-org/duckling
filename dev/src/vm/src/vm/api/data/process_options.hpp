#pragma once

namespace vm::api {
	enum class ProcessMode { Safe, Fast };

	struct ProcessOptions {
		ProcessMode mode = ProcessMode::Safe;
	};
}
