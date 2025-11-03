#include <base/str/str_utils.hpp>

#include <random>

void base::internal::strConcat(std::string& out, const icu::UnicodeString& unistr) {
	unistr.toUTF8String(out);
}

void base::strReplaceAll(std::string& str, const std::string& from, const std::string& to) {
	// Thanks to: https://stackoverflow.com/a/3418285.
	if (from.empty()) return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

std::vector<std::string> base::strSplit(const std::string_view str, const std::string& delimiter) {
	std::vector<std::string> result;
	size_t                   end_pos   = 0;
	size_t                   start_pos = 0;
	while ((end_pos = str.find(delimiter, start_pos)) != std::string::npos) {
		result.emplace_back(str.substr(start_pos, end_pos - start_pos));
		start_pos = end_pos + delimiter.length();
	}
	result.emplace_back(str.substr(start_pos));
	return result;
}

/**
 * @brief Generates a random alphanumeric string of the specified length.
 * @param length The length of the random string to generate.
 * @return A random alphanumeric string.
 */
std::string base::generateRandomString(size_t length) {
	static constexpr std::string_view CHARS
		= "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static std::random_device                    rd;
	static std::mt19937                          generator(rd());
	static std::uniform_int_distribution<size_t> distribution(0, CHARS.size() - 1);

	std::string random_string;
	random_string.reserve(length);
	for (size_t i = 0; i < length; ++i) random_string += CHARS[distribution(generator)];
	return random_string;
}
