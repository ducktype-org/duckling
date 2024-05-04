#include <base/str_utils.hpp>

void base::strReplaceAll(std::string& str, const std::string& from, const std::string& to) {
	// Thanks to: https://stackoverflow.com/a/3418285.
	if (from.empty()) return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

std::vector<std::string> base::strSplit(const std::string& str, const std::string& delimeter) {
	std::vector<std::string> result;
	size_t                   end_pos   = 0;
	size_t                   start_pos = 0;
	while ((end_pos = str.find(delimeter, start_pos)) != std::string::npos) {
		result.push_back(str.substr(start_pos, end_pos - start_pos));
		start_pos = end_pos + delimeter.length();
	}
	result.push_back(str.substr(start_pos));
	return result;
}
