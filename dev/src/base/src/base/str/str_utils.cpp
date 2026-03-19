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
std::string base::generateRandomString(u64 length) {
	static constexpr std::string_view CHARS
		= "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static std::random_device                 rd;
	static std::mt19937                       generator(rd());
	static std::uniform_int_distribution<u64> distribution(0, CHARS.size() - 1);

	std::string random_string;
	random_string.reserve(length);
	for (u64 i = 0; i < length; ++i) random_string += CHARS[distribution(generator)];
	return random_string;
}

base::UnescapeResult base::unescapeString(const std::string_view raw) {
	std::string result;
	result.reserve(raw.size());

	for (usize i = 0; i < raw.size(); ++i) {
		if (raw[i] == '\\' && i + 1 < raw.size()) {
			// Peek at the next character
			// clang-format off
			switch (raw[i + 1]) {
			case 'n':  result += '\n'; break; // Newline
			case 'r':  result += '\r'; break; // Carriage return
			case 't':  result += '\t'; break; // Tab
			case 'v':  result += '\v'; break; // Vertical tab
			case 'b':  result += '\b'; break; // Backspace
			case 'f':  result += '\f'; break; // Form feed
			case 'a':  result += '\a'; break; // Alert (bell)
			case 'e':  result += '\033'; break; // Escape (non-standard but common)
			case '\\': result += '\\'; break; // Literal backslash
			case '\"': result += '\"'; break; // Double quote
			case '\'': result += '\''; break; // Single quote
			case '0':  result += '\0'; break; // Null character

			default:
				return std::unexpected(UnknownEscapeSequence{strConcat("\\", raw[i + 1])});
			}
			// clang-format on
			i++;  // Skip the escaped character
		} else {
			result += raw[i];
		}
	}
	return UnescapedString{ result };
}

std::string base::escapeString(std::string_view raw) {
	std::string result;
	result.reserve(raw.size() * 2);  // Worst case: every character needs escaping

	for (char c: raw) {
		// clang-format off
		switch (c) {
		case '\n': result += "\\n"; break; // Newline
		case '\r': result += "\\r"; break; // Carriage return
		case '\t': result += "\\t"; break; // Tab
		case '\v': result += "\\v"; break; // Vertical tab
		case '\b': result += "\\b"; break; // Backspace
		case '\f': result += "\\f"; break; // Form feed
		case '\a': result += "\\a"; break; // Alert (bell)
		case '\033': result += "\\e"; break; // Escape (non-standard but common)
		case '\\': result += "\\\\"; break; // Literal backslash
		case '\"': result += "\\\""; break; // Double quote
		case '\'': result += "\\\'"; break; // Single quote
		case '\0': result += "\\0"; break;  // Null character

		default:
			result += c;
			break;
		}
		// clang-format on
	}
	return result;
}
