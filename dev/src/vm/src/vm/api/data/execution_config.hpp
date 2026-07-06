#pragma once

namespace vm::api {
	/**
	 * A set of execution configuration flags stored on the VM/process and consulted during
	 * execution-related operations such as loading and validation. Not all operations use all
	 * flags. Default-initialized (all flags false) describes an unrestricted run.
	 */
	struct ExecutionConfig {
		/// Code execution cannot perform any IO, e.g. when performing compile-time evaluation.
		bool no_io = false;

		/// Code execution cannot modify a global state, e.g. when performing compile-time evaluation.
		bool read_only = false;

		/// Process can only have a single thread — user cannot spawn more
		bool single_thread = false;
	};
}
