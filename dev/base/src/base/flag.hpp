/**
 * @file flag.hpp
 * @brief This is a very basic implementation of FlagType, which is enum-like
 * type, that allow to store multiple options in single value.
 *
 * @note Current implementation does not allow to create strongly
 * typed flag-types. It should be changed to this in the future.
 */
#pragma once

#include "ints.hpp"
#include <compare>
#include <sstream>
#include "stringifyable_enum.hpp"

#define MAKE_FLAG_TYPE(namespace_name, enum_name, flag_name, ...)                              \
	MAKE_STRINGIFYABLE_ENUM(namespace_name, u32, enum_name, __VA_ARGS__)                       \
                                                                                               \
	namespace namespace_name {                                                                 \
		class flag_name {                                                                      \
			u64 data = 0;                                                                      \
                                                                                               \
			[[nodiscard]] constexpr flag_name(u64 data): data(data) {}                         \
                                                                                               \
		public:                                                                                \
			[[nodiscard]] constexpr flag_name() = default;                                     \
                                                                                               \
			[[nodiscard]] constexpr flag_name(enum_name single_option):                        \
				  data(1 << static_cast<u32>(single_option)) {}                                \
                                                                                               \
			[[nodiscard]]                                                                      \
			constexpr bool contains(const flag_name& oth) const {                              \
				return (data & oth.data) == oth.data;                                          \
			}                                                                                  \
                                                                                               \
			[[nodiscard]]                                                                      \
			constexpr bool contains(const enum_name& single_option) const {                    \
				return contains(flag_name(single_option));                                     \
			}                                                                                  \
                                                                                               \
			constexpr flag_name operator|(const flag_name& oth) const {                        \
				return flag_name(data | oth.data);                                             \
			}                                                                                  \
                                                                                               \
			constexpr flag_name operator|(const enum_name& oth) const {                        \
				return operator|(flag_name(oth));                                              \
			}                                                                                  \
                                                                                               \
			constexpr flag_name operator&(const flag_name& oth) {                              \
				return flag_name(data & oth.data);                                             \
			}                                                                                  \
                                                                                               \
			constexpr flag_name operator&(const enum_name& oth) {                              \
				return operator&(flag_name(oth));                                              \
			}                                                                                  \
                                                                                               \
			constexpr void operator|=(const flag_name& oth) { data |= oth.data; }              \
                                                                                               \
			constexpr void operator|=(const enum_name& oth) { operator|=(flag_name(oth)); }    \
                                                                                               \
			constexpr void operator&=(const flag_name& oth) { data &= oth.data; }              \
                                                                                               \
			constexpr void operator&=(const enum_name& oth) { operator&=(flag_name(oth)); }    \
                                                                                               \
			constexpr bool operator==(const flag_name& oth) const { return data == oth.data; } \
                                                                                               \
			constexpr auto operator<=>(const flag_name& oth) const = default;                  \
                                                                                               \
			[[nodiscard]]                                                                      \
			std::string to_string() const {                                                    \
				std::stringstream ss;                                                          \
				std::string       separator;                                                   \
                                                                                               \
				for (int i = 0; i < static_cast<u32>(enum_name::COUNT); i++) {                 \
					if (data & (1 << i)) {                                                     \
						ss << separator << base::enumToStr(static_cast<enum_name>(i)).str();   \
						separator = " | ";                                                     \
					}                                                                          \
				}                                                                              \
                                                                                               \
				return ss.str();                                                               \
			}                                                                                  \
		};                                                                                     \
                                                                                               \
		inline flag_name operator|(const enum_name& single_option, const flag_name& oth) {     \
			return flag_name(single_option) | oth;                                             \
		}                                                                                      \
                                                                                               \
		inline flag_name operator|(const enum_name& single_option, const enum_name& oth) {     \
			return flag_name(single_option) | oth;                                             \
		}                                                                                      \
	}
