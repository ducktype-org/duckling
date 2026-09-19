#pragma once

namespace vm::api {
	enum class ProcessMode { Safe, Fast };

	struct ProcessConfig final {
		ProcessMode mode                      = ProcessMode::Safe;
		bool        enable_deadlock_detection = false;
		/// Only meaningful for the Safe mode in JIT builds; ignored otherwise.
		bool enable_jit = true;
	};
}
