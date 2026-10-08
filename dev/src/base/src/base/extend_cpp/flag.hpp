// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file flag.hpp
 * @brief This is a very basic implementation of FlagType, which is enum-like
 * type, that allow to store multiple options in single value.
 */
#pragma once

#include <base/extend_cpp/stringifyable_enum.hpp>  // IWYU pragma: export
#include <base/types/ints.hpp>                     // IWYU pragma: export
#include <base/types/monostate.hpp>                // IWYU pragma: export

#include <compare>                                 // IWYU pragma: export
#include <sstream>                                 // IWYU pragma: export

#define MAKE_FLAG_TYPE(namespace_name, enum_name, flag_name, ...)                                    \
	MAKE_STRINGIFYABLE_ENUM(namespace_name, u32, enum_name, __VA_ARGS__)                                                                          \
                                                                                                     \
	static_assert(                                                                                   \
		static_cast<u32>(namespace_name::enum_name::COUNT) < 64, "Too many flag options."            \
	);                                                                                               \
                                                                                                     \
	namespace namespace_name {                                                                       \
		class flag_name {                                                                            \
			u64 data = 0;                                                                            \
                                                                                                     \
			[[nodiscard]] constexpr flag_name(u64 data): data(data) {}                               \
                                                                                                     \
		public:                                                                                      \
			static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};                \
			[[nodiscard]] constexpr flag_name()                                 = default;           \
                                                                                                     \
			[[nodiscard]] constexpr flag_name(enum_name single_option):                              \
				  data(1 << static_cast<u32>(single_option)) {}                                      \
                                                                                                     \
			[[nodiscard]]                                                                            \
			constexpr bool contains(const flag_name& oth) const {                                    \
				return (data & oth.data) == oth.data;                                                \
			}                                                                                        \
                                                                                                     \
			[[nodiscard]]                                                                            \
			constexpr bool contains(const enum_name& single_option) const {                          \
				return contains(flag_name(single_option));                                           \
			}                                                                                        \
                                                                                                     \
			constexpr flag_name operator|(const flag_name& oth) const {                              \
				return flag_name(data | oth.data);                                                   \
			}                                                                                        \
                                                                                                     \
			constexpr flag_name operator|(const enum_name& oth) const {                              \
				return operator|(flag_name(oth));                                                    \
			}                                                                                        \
                                                                                                     \
			constexpr flag_name operator&(const flag_name& oth) {                                    \
				return flag_name(data & oth.data);                                                   \
			}                                                                                        \
                                                                                                     \
			constexpr flag_name operator&(const enum_name& oth) {                                    \
				return operator&(flag_name(oth));                                                    \
			}                                                                                        \
                                                                                                     \
			constexpr void operator|=(const flag_name& oth) { data |= oth.data; }                    \
                                                                                                     \
			constexpr void operator|=(const enum_name& oth) { operator|=(flag_name(oth)); }          \
                                                                                                     \
			constexpr void operator&=(const flag_name& oth) { data &= oth.data; }                    \
                                                                                                     \
			constexpr void operator&=(const enum_name& oth) { operator&=(flag_name(oth)); }          \
                                                                                                     \
			constexpr bool operator==(const flag_name& oth) const { return data == oth.data; }       \
                                                                                                     \
			constexpr auto operator<=>(const flag_name& oth) const = default;                        \
			constexpr void operator-=(const flag_name& oth) { data &= ~oth.data; }                   \
			constexpr void operator-=(const enum_name& oth) { operator-=(flag_name(oth)); }          \
                                                                                                     \
			[[nodiscard]]                                                                            \
			std::string toString(bool in_brackets = false) const {                                   \
				std::stringstream ss;                                                                \
				std::string_view  separator = "";                                                    \
                                                                                                     \
				if (in_brackets) ss << "[";                                                          \
				for (int i = 0; i < static_cast<u32>(enum_name::COUNT); i++) {                       \
					if (data & (1 << i)) {                                                           \
						ss << separator << base::enumToStr(static_cast<enum_name>(i));               \
						separator = "|";                                                             \
					}                                                                                \
				}                                                                                    \
				if (in_brackets) ss << "]";                                                          \
                                                                                                     \
				return ss.str();                                                                     \
			}                                                                                        \
		};                                                                                           \
                                                                                                     \
		constexpr inline flag_name operator|(const enum_name& single_option, const flag_name& oth) { \
			return flag_name(single_option) | oth;                                                   \
		}                                                                                            \
                                                                                                     \
		constexpr inline flag_name operator|(const enum_name& single_option, const enum_name& oth) { \
			return flag_name(single_option) | oth;                                                   \
		}                                                                                            \
	}
