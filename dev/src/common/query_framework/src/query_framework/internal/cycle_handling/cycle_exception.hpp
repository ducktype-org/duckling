#pragma once

#include <exception>

namespace query::internal {

	/**
	 * Special exception that is thrown when a cycle is detected during the query invocation.
	 * This is used to break the provide() execution, as given query computation can't continue
	 * after cyclic call (this is a requirement of a Query Model we implement).
	 */
	class QueryCycleException final: public std::exception {
	public:
		[[nodiscard]] const char* what() const noexcept final {
			return "Query cycle detected. This exception is thrown when a cycle is detected in the query graph. Note that this message should not ever be called.";
		}
	};
}
