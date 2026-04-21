#pragma once

#include <utility>

#include <base/types/checked_okbad.hpp>

#include <query_framework/internal/query_errors.hpp>

namespace query {

	/**
	 * @brief Runs a function with query failed exception handling.
	 * @param func The function to run.
	 * @return base::OK if QueryFailedException is not thrown, base::BAD otherwise.
	 */
	template<typename FuncT>
	base::CheckedOkBad runFuncWithQueryFailedHandling(FuncT&& func) {
		try {
			std::forward<FuncT>(func)();
			return base::OK;
		} catch (const query::internal::QueryFailedException&) {
			// We catch the exception to prevent it from propagating further, but we don't do
			// anything with it here. The callee is expected to check this case and handle it
			// appropriately (for example, by returning query::Failed() or doing some cleanup).
			return base::BAD;
		}
	}
}
