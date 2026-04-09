#include "bit256.hpp"

#include <ranges>

namespace base {
	Bit256::Bit256(std::string_view hex): Bit256() {
		CORE_ASSERT(hex.size() <= 66, "Bit256 string too long");
		CORE_ASSERT(
			hex.starts_with("0x") || hex.starts_with("0X"), "Bit256 string must start with 0x"
		);
		// Remove "0x" prefix
		hex = hex.substr(2);
		for (std::size_t i = 0; hex.size() > 0; ++i) {
			const auto len = std::min(16uz, hex.size());
			auto       end = hex.substr(hex.size() - len, len);
			hex            = hex.substr(0, hex.size() - len);

			u64 value = 0;
			for (const char c: end) {
				value <<= 4;
				if (c >= '0' && c <= '9')
					value |= static_cast<u64>(c - '0');
				else if (c >= 'a' && c <= 'f')
					value |= static_cast<u64>(c - 'a' + 10);
				else if (c >= 'A' && c <= 'F')
					value |= static_cast<u64>(c - 'A' + 10);
				else
					CORE_ASSERT(
						false, "Invalid character \'" + std::string(1, c) + "\' in Bit256 string"
					);
			}
			data.at(i) = value;
		}
	}

	std::string Bit256::toStringHex() const {
		std::string ret;
		ret.reserve(64);
		for (const auto& d: std::views::reverse(data))
			for (int j = 0; j < 16; ++j)
				ret += std::string_view("0123456789abcdef").at(((d >> (60 - j * 4)) & 0xF));
		return ret;
	}

	std::ostream& operator<<(std::ostream& os, const base::Bit256& bit256) {
		std::string ret;
		ret += '{';
		for (size_t i = 0; i < bit256.data.size(); ++i) {
			ret += std::to_string(bit256.data.at(i));
			if (i < bit256.data.size() - 1) ret += ", ";
		}
		ret += '}';
		os << ret;
		return os;
	}
}
