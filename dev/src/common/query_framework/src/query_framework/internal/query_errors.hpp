#pragma once

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <iostream>

namespace query::internal {
	/**
	 * @brief Exception that can be thrown when accessing a result of a query that has failed.
	 * It can be caught by the enclosing query from query framework.
	 */
	class QueryFailedException final: public base::Exception {
		std::string what_str;

	public:
		QueryFailedException(std::string_view reason) {
			what_str = std::string("Query failure not handled: ");
			what_str += std::string(reason) + "\n\n";

			IF_BUILD_TYPE_DEV({
				what_str += "Stacktrace:\n";
				what_str += base::getCurrentStackTrace(6);

				// std::cerr << "stacktrace!\n";
			});
		}

		[[nodiscard]] const char* what() const noexcept final { return what_str.c_str(); }
	};

}
