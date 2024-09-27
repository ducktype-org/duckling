#pragma once

#include "../query_int.hpp"

#include <tester/tester.hpp>

#include <functional>

namespace tester {
	/**
	 * @brief A TestSuite with additional functionality of calling function objects
	 * with query::Context via the `withContextDo` method.
	 *
	 * @note Use only in tests (obviously).
	 */
	class ContextSuite: public TestSuite {
	protected:
		ContextSuite(TestConfig&& config, const std::string& name):
			  TestSuite(std::move(config), name) {}

		/**
		 * @brief Execute a function as if it were in the middle of a query, i.e. supplied with a
		 * query::Context. The function may return a value of any copy-constructible type.
		 *
		 * @note If you are supplying a lambda, make sure it catches `this` be reference
		 * ([&] works), to enable use of TestSuite methods and avoid unhelpful errors.
		 *
		 * @param action The function object to execute, applied to a query::Context.
		 * The function may return a value of any copy-constructible type.
		 *
		 * @return The result of the function. Possibly empty.
		 */
		static std::any withContextCompute(std::function<std::any(query::Context&)>);

		/**
		 * @brief Execute a procedure as if it were in the middle of a query, i.e. supplied with a
		 * query::Context.
		 *
		 * @note If you are supplying a lambda, make sure it catches `this` be reference
		 * ([&] works), to enable use of TestSuite methods and avoid unhelpful errors.
		 *
		 * @param action The procedure object to execute, applied to a query::Context.
		 */
		static void withContextDo(std::function<void(query::Context&)>);
	};
}
