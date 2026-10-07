// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <exception>

namespace query::internal {

	/**
	 * Special exception that is thrown when a cycle is detected during the query invocation.
	 * This is used to break the provide() execution, as given query computation can't continue
	 * after cyclic call (this is a requirement of a DuckLing Query Model).
	 */
	class QueryCycleException final: public std::exception {
	public:
		[[nodiscard]] const char* what() const noexcept final {
			return "Query cycle detected. This exception is thrown when a cycle is detected in the "
				   "query graph. Note that this message should not ever be called.";
		}
	};
}
