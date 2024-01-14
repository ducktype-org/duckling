#include "../str_concat.hpp"

namespace base::detail {
	void strConcat(std::string& out, icu::UnicodeString unistr) { unistr.toUTF8String(out); }
}
