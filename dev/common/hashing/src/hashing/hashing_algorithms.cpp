#include <iostream>
#include <iomanip>
#include <sstream>

// #include <base/ints.hpp>
#include "../../../../base/src/base/ints.hpp"


#include "hashing_algorithms.hpp"

namespace hashing {

	debug_hash::operator debug_hash::result_type() noexcept {
		std::stringstream ss;
		usize             line = 0, pos = 0;

		for (auto&& [b, len, type]: bytes) {
			for (usize i = pos, j = 0; j < len; ++i, ++j) {
				if (i % 16 == 0) {
					if (line != 0) ss << '\n';
					ss << "line " << std::setw(4) << std::setfill(' ') << line++ << ":    ";
				}
				if (i == pos) {
					if (type == type::hash_code)
						ss << "\033[1;33m";
					else
						ss << "\033[1;31m";
				}
				ss << std::hex << std::setw(2) << std::setfill('0')
				   << static_cast<u16>(static_cast<std::byte>(b[j])) << std::dec << ' ';
				if (i == pos) ss << "\033[0m";
			}
			pos = (pos + len) % 16;
		}

		return ss.str();
	}

}  // namespace hashing
