#pragma once

#include "options.hpp"

#include <base/pointers/ref.hpp>

namespace global_state {

	/**
	 * Returns the current backend options for compilation.
	 */
	CRef<compiler::options::BackendOptions> getBackendOptions();

	namespace setters {
		/**
		 * Sets the backend options for compilation.
		 */
		void setBackendOptions(compiler::options::BackendOptions options);
	}
}
