#pragma once

#include <base/types/ints.hpp>

namespace concurrent {

	enum class CmpRes {
		Changed,
		NotChanged,
	};

	// constexpr u64 pow(u64 base, u64 exp) {
	//     u64 result = 1;
	//     while (exp > 0) {
	//         if (exp % 2 == 1) {
	//             result *= base;
	//         }
	//         exp /= 2;
	//         base *= base;
	//     }
	//     return result;
	// }

}
