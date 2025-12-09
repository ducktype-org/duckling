#pragma once

#include <base/except/exceptions.hpp>

namespace query {
	class Failed final {};

	class QueryFailedException final: public base::Exception {
		std::string what_str;

	public:
		QueryFailedException(std::string_view reason) {
			what_str = std::string("Query failure not handled: ");
			what_str += std::string(reason) + "\n\n";
			what_str += "Stacktrace:\n";
			what_str += base::getCurrentStackTrace();
		}

		[[nodiscard]] const char* what() const noexcept final { return what_str.c_str(); }
	};

	inline void throwFailed() { throw QueryFailedException("Query failure"); }
}
