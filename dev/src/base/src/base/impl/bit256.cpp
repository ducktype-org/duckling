#include "../bit256.hpp"

#include <base/ints.hpp>

#include <cstring>
#include <stdexcept>
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

	std::vector<uint8_t> Bit256::serialize() const {
		std::vector<uint8_t> buffer(sizeof(data));
		std::memcpy(buffer.data(), data.data(), sizeof(data));
		return buffer;
	}

	Bit256 Bit256::deserialize(const std::vector<uint8_t>& buffer, const usize& offset) {
		if (offset + sizeof(decltype(Bit256::data)) > buffer.size())
			throw std::out_of_range("Buffer size exceeded during Bit256 deserialization");

		Bit256 bit256;
		std::memcpy(bit256.data.data(), buffer.data() + offset, sizeof(bit256.data));
		return bit256;
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
