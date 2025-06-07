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
		std::string ret;
		ret += '{';
		for (size_t i = 0; i < bit256.data.size(); ++i) {
			ret += std::to_string(bit256.data.at(i));
			if (i < bit256.data.size() - 1) {
				ret += ", ";
			}
		}
		ret += '}';
		os << ret;
		return os;
	}
}
