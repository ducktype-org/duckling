#include <base/preproc/argument_splitter.hpp>
#include <base/types/ints.hpp>

#include <sstream>

namespace base {
	// @TODO: this solution is somewhat over engineered
	// Maybe use base::strSplit()?
	std::vector<std::string> vaArgSplit(std::string_view va_arg) {
		std::vector<std::string> out;
		auto                     len = va_arg.length();

		std::ostringstream helper;

		for (usize i = 0; i < len; i++)
			if (std::isspace(va_arg[i]))
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
