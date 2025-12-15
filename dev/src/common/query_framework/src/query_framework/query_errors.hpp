#pragma once

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

namespace query {
	/**
	 * @brief The Failed class is used as a marker to indicate that a query has failed.
	 * It is returned by queries in QResult.
	 */
	class Failed final {};

	/**
	 * @brief Exception that can be thrown when accessing a query that has failed.
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
				what_str += base::getCurrentStackTrace();
			});
		}

		[[nodiscard]] const char* what() const noexcept final { return what_str.c_str(); }
	};

	inline void throwFailed() { throw QueryFailedException("Query failure"); }
}
