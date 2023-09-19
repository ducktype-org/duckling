#include "../define_helper.hpp"

#include "../ints.hpp"

#include <sstream>

namespace base {
	// @TODO: this solution is somewhat over engineered
	std::vector<std::string> vaArgSplit(std::string_view va_arg) {
		std::vector<std::string> out;
		auto                     len = va_arg.length();

		std::ostringstream helper;

		for (usize i = 0; i < len; i++)
			if (isspace(va_arg[i]))
				continue;
			else if (va_arg[i] == ',') {
				out.push_back(helper.str());
				helper.str(std::string());
				helper.clear();
			} else
				helper << va_arg[i];
		out.push_back(helper.str());
		helper.clear();

		return out;
	}
}
