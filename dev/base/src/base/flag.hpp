#pragma once

#include "ints.hpp"
#include <compare>

namespace base {
	/**
	 * @brief Simple flag type. It stores up to 64 bits
	 */
	class FlagType {
		u64 data = 0;

		constexpr static FlagType makeFlag(usize data) {
			FlagType out;
			out.data = data;
			return out;
		}

	public:
		constexpr FlagType() = default;

		[[nodiscard]] 
		constexpr bool contains(const FlagType& oth) const { return (data & oth.data) == oth.data; }

		constexpr FlagType operator|(const FlagType& oth) const {
			return makeFlag(data | oth.data);
		}

		constexpr void operator|=(const FlagType& oth) { data |= oth.data; }

		constexpr bool operator==(const FlagType& oth) const { return data == oth.data; }

		constexpr auto operator<=>(const FlagType& oth) const = default;

		constexpr FlagType(u64 flag_id): data{ 1ull << flag_id } {}
	};

	/**
	 * @brief Flag with everything set to 0.
	 */
	constexpr FlagType EmptyFlag;
}
