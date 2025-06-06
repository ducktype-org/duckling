#include "../bit256.hpp"

#include <string_view>

namespace base {
	std::string Bit256::toStringHex() const {
		std::string ret;
		ret.reserve(64);
		for (const auto& d: data)
			for (int i = 0; i < 16; ++i)
				ret += std::string_view("0123456789abcdef").at(((d >> (60 - i * 4)) & 0xF));
		return ret;
	}
}

namespace std {
	std::ostream& operator<<(std::ostream& os, const base::Bit256& bit256) {
		return os << bit256.toStringHex();
	}
}
