#pragma once

#include "../query_impl.hpp"

#include <tester/tester.hpp>

#include <functional>

namespace tester {
	/**
	 * @brief A TestSuite with additional functionality of calling function objects
	 * with query::Context via the `withContext` method.
	 *
	 * @note Use only in tests (obviously).
	 */
	class ContextSuite: public TestSuite {
	protected:
		ContextSuite(TestConfig&& config, const std::string& name):
			  TestSuite(std::move(config), name) {}

		/**
		 * @brief Execute a function as if it were in the middle of a query, i.e. supplied with a
		 * query::Context. The function returns a pointer to a result of unchecked type.
		 *
		 * @param action The function object to execute, applied to a query::Context.
		 * The function *must* return a `void*` to its result, but it can be a nullptr if the
		 * result is empty.
		 *
		 * @return The result of the function. Possibly nullptr.
		 *
		 * @note The caller is responsible for freeing the memory under the returned pointer.
		 */
		static void* withContext(std::function<void*(query::Context&)>);
	};
}
