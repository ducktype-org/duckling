#pragma once

#include "../query_int.hpp"
#include <functional>
#include <any>

namespace query::utils {
	/**
	 * @brief Execute a procedure as if it were in the middle of a query, i.e. supplied with a
	 * query::Context.
	 *
	 * @note It uses query::entryPoint
	 *
	 * @note When used in tests, if you are supplying a lambda, make sure it catches `this` be reference
	 * ([&] works), to enable use of TestSuite methods and avoid unhelpful errors.
	 *
	 * @param action The procedure object to execute, applied to a query::Context.
	*/
	std::any withContextCompute(std::function<std::any(query::Context&)>);
	
	/**
	 * @brief Execute a function as if it were in the middle of a query, i.e. supplied with a
	 * query::Context. The function may return a value of any copy-constructible type.
	 *
	 * @note It uses query::entryPoint
	 *
	 * @note When used in tests, if you are supplying a lambda, make sure it catches `this` be reference
	 * ([&] works), to enable use of TestSuite methods and avoid unhelpful errors.
	 *
	 * @param action The function object to execute, applied to a query::Context.
	 * The function may return a value of any copy-constructible type.
	 *
	 * @return The result of the function. Possibly empty.
	 */
	void withContextDo(std::function<void(query::Context&)>);
}
