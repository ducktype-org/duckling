/**
 * @file strongly_typed_id.hpp
 *
 * @brief Strongly typed id is a library that provides macros that create a class
 * implementing a strongly typed ID type.
 */
#pragma once

#include "ints.hpp"  // IWYU pragma: export

/**
 * @brief Macro used to create Strong ID types.
 * Usage:
 * 	STRONG_TYPEDEF_ID(TypeName)
 *
 *  Created type has following interface:
 *
 *  * Type() - default constructor creating bad ID
 *  * Type::next() - get next id
 *  * Type::bad() - get bad id
 *  * id.isBad(), id.idGood() - check if given ID is good/bad
 *  * id.asInt() - get underlying integer
 *  * <=>, <, ==, etc - all standard comparision operators
 *
 * @note it creates normal class, it can be used in namespace
 */
#define STRONG_TYPEDEF_ID(NAME)                                          \
	class NAME final {                                                   \
	private:                                                             \
		inline static u64    NEXT_ID = 0;                                \
		constexpr static u64 BAD_ID  = u64(-1);                          \
		u64                  id      = BAD_ID;                           \
		inline constexpr NAME(u64 id): id{ id } {}                       \
                                                                         \
	public:                                                              \
		inline constexpr NAME()                               = default; \
		inline constexpr NAME(const NAME& mX)                 = default; \
		inline constexpr NAME(NAME&& mX) noexcept             = default; \
		inline constexpr NAME& operator=(const NAME& rhs)     = default; \
		inline constexpr NAME& operator=(NAME&& rhs) noexcept = default; \
		[[nodiscard]]                                                    \
		static NAME next() {                                             \
			NAME out;                                                    \
			out.id = NAME::NEXT_ID++;                                    \
			return out;                                                  \
		}                                                                \
		static NAME bad() { return NAME{ BAD_ID }; }                     \
		[[nodiscard]]                                                    \
		inline constexpr explicit operator u64() const noexcept {        \
			return id;                                                   \
		}                                                                \
		[[nodiscard]]                                                    \
		inline constexpr u64 asInt() const noexcept {                    \
			return id;                                                   \
		}                                                                \
		auto        operator<=>(const NAME&) const = default;            \
		inline bool isBad() const { return id == BAD_ID; }               \
		inline bool isGood() const { return id != BAD_ID; }              \
	};

/**
 * @brief Add std::hash specialization to given ID type.
 * Usage: ID_STD_HASH(MY_ID)
 */
#define ID_STD_HASH(TYPE)                                                           \
	template<>                                                                      \
	struct std::hash<TYPE> final {                                                  \
		usize operator()(const TYPE& key) const { return static_cast<usize>(key.asInt()); } \
	};
