#pragma once

#include <base/collections/optional.hpp>

namespace vm::api {
	/**
	 * A set of configuration flags to pass with each request. Not all requests use all flags.
	 */
	struct ExecutionConfig {
		// Code execution cannot perform any IO, e.g. when performing compile-time evaluation.
		base::Optional<bool> no_io;

		// Code execution cannot modify a global state, e.g. when performing compile-time evaluation.
		base::Optional<bool> read_only;

		// The user cannot spawn threads, e.g. in a REPL environment.
		base::Optional<bool> single_thread;
	};
}
