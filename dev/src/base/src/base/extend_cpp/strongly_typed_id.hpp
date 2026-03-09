/**
 * @file strongly_typed_id.hpp
 *
 * @brief Strongly typed id is a library that provides macros that create a class
 * implementing a strongly typed ID type.
 */
#pragma once

#include <base/types/ints.hpp>  // IWYU pragma: export

#include <atomic>               // IWYU pragma: export
#include <compare>              // IWYU pragma: export

/**
 * @brief Macro used to create Strong ID types.
 * Usage:
 * 	STRONG_TYPEDEF_ID(TypeName);
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
#define STRONG_TYPEDEF_ID(NAME)                                              \
	class NAME final {                                                       \
	private:                                                                 \
		inline static std::atomic<u64> NEXT_ID = 0;                          \
		constexpr static u64           BAD_ID  = u64(-1);                    \
		u64                            id      = BAD_ID;                     \
		inline constexpr NAME(u64 id): id{ id } {}                           \
                                                                             \
	public:                                                                  \
		inline constexpr NAME()                               = default;     \
		inline constexpr NAME(const NAME& mX)                 = default;     \
		inline constexpr NAME(NAME&& mX) noexcept             = default;     \
		inline constexpr NAME& operator=(const NAME& rhs)     = default;     \
		inline constexpr NAME& operator=(NAME&& rhs) noexcept = default;     \
		[[nodiscard]]                                                        \
		static NAME next() {                                                 \
			NAME out;                                                        \
			out.id = NAME::NEXT_ID.fetch_add(1, std::memory_order_relaxed);  \
			return out;                                                      \
		}                                                                    \
		static NAME bad() { return NAME{ BAD_ID }; }                         \
		[[nodiscard]]                                                        \
		inline constexpr explicit operator u64() const noexcept {            \
			return id;                                                       \
		}                                                                    \
		[[nodiscard]]                                                        \
		inline constexpr u64 asInt() const noexcept {                        \
			return id;                                                       \
		}                                                                    \
		inline constexpr u64 queryUnstablePerfectHash() const { return id; } \
		auto                 operator<=>(const NAME&) const = default;       \
		inline bool          isBad() const { return id == BAD_ID; }          \
		inline bool          isGood() const { return id != BAD_ID; }         \
	}

/**
 * @brief Macro used to create Strong ID types that can be created
 * directly from integers rather then with `Type::next()`
 * Usage:
 * 	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeName);
 *
 *  Created type has following interface:
 *
 *  * Type() - default constructor creating bad ID
 *  * Type::bad() - get bad id
 *  * Type::fromU64(v), Type(v) - creates id from u64
 *  * id.isBad(), id.idGood() - check if given ID is good/bad
 *  * id.asInt() - get underlying integer
 *  * <=>, <, ==, etc - all standard comparision operators
 *  *
 *
 * @note it creates normal class, it can be used in namespace
 */
#define STRONG_TYPEDEF_ID_DIRECT_CREATION(NAME)                          \
	class NAME final {                                                   \
	private:                                                             \
		constexpr static u64 BAD_ID = u64(-1);                           \
		u64                  id     = BAD_ID;                            \
                                                                         \
	public:                                                              \
		inline constexpr explicit NAME(u64 id): id{ id } {}              \
		inline constexpr NAME()                               = default; \
		inline constexpr NAME(const NAME& mX)                 = default; \
		inline constexpr NAME(NAME&& mX) noexcept             = default; \
		inline constexpr NAME& operator=(const NAME& rhs)     = default; \
		inline constexpr NAME& operator=(NAME&& rhs) noexcept = default; \
		static NAME            bad() { return NAME{ BAD_ID }; }          \
		static NAME            fromU64(u64 v) { return NAME{ v }; }      \
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
	}


/**
 * @brief Add std::hash specialization to given ID type.
 * Usage: ID_STD_HASH(MY_ID);
 */
#define ID_STD_HASH(TYPE)                                                                   \
	template<>                                                                              \
	struct std::hash<TYPE> final {                                                          \
		usize operator()(const TYPE& key) const { return static_cast<usize>(key.asInt()); } \
	}
