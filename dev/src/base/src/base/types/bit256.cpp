#include "bit256.hpp"

namespace base {
	std::string Bit256::toStringHex() const {
		std::string ret;
		ret.reserve(64);
		for (std::size_t i = data.size() - 1; i < data.size(); --i) {
			const auto& d = data.at(i);
			for (int j = 0; j < 16; ++j)
				ret += std::string_view("0123456789abcdef").at(((d >> (60 - j * 4)) & 0xF));
		}
		return ret;
	}

	Bit256 operator*(const Bit256& lhs, const Bit256& rhs) noexcept {
#if defined(__SIZEOF_INT128__)
		Bit256 ret{};
		for (usize i = 0; i < lhs.data.size(); ++i) {
			__uint128_t carry = 0;
			for (usize j = 0; j < rhs.data.size(); ++j) {
				if (i + j >= rhs.data.size()) break;
				const __uint128_t sum = static_cast<__uint128_t>(lhs.data.at(i)) * rhs.data.at(j)
				                      + ret.data.at(i + j) + carry;
				ret.data.at(i + j) = static_cast<u64>(sum);
				carry              = sum >> 64;
			}
		}
		return ret;
#else
		std::array<u32, 8> a{}, b{};
		for (size_t i = 0; i < 4; ++i) {
			a.at(2 * i)     = static_cast<u32>(lhs.data.at(i));
			a.at(2 * i + 1) = static_cast<u32>(lhs.data.at(i) >> 32);
			b.at(2 * i)     = static_cast<u32>(rhs.data.at(i));
			b.at(2 * i + 1) = static_cast<u32>(rhs.data.at(i) >> 32);
		}

		std::array<u32, 8> ret_halves{};
		for (size_t i = 0; i < 8; ++i) {
			u64 carry = 0;
			for (size_t j = 0; j < 8; ++j) {
				if (i + j >= 8) break;
				const u64 sum = static_cast<u64>(a.at(i)) * b.at(j) + ret_halves.at(i + j) + carry;
				ret_halves.at(i + j) = static_cast<u32>(sum);
				carry                = sum >> 32;
			}
		}

		for (size_t i = 0; i < 4; ++i) {
			ret.data.at(i) = static_cast<u64>(ret_halves.at(2 * i))
			               | (static_cast<u64>(ret_halves.at(2 * i + 1)) << 32);
		}
		return ret_halves;
#endif
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
