#include "../stringifyable_enum.hpp"
#include "../exceptions.hpp"

namespace base::detail {
	void validateStrEnumVaArgs(std::string_view va_args) {
		char previous = ' ';
		for (auto c: va_args) {
			CORE_ASSERT(c != '-', "VaArgs for stringifyable enum cannot contain '-' character");
			CORE_ASSERT(c != '=', "VaArgs for stringifyable enum cannot contain default values");
			if (std::isdigit(c)) {
				if (not std::isalnum(previous)) {
					// there is a digit that is not a part of identifier:
					CORE_PANIC("A number is present in VaArgs passed to stringifyable enum");
				}
			}
			previous = c;
		}
	}
}
