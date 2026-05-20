/**
 * @file strongly_typed_int.hpp
 *
 * @brief Provides macros for creating a strongly typed integer.
 * They are created as classes that generally behave the same as other integral types but can only
 * be explicitly cast.
 *
 * Functionalities
 * ---------------
 *
 * Two variants are provided:
 *
 * - STRONG_TYPEDEF_INT_DIMENSIONAL
 * - STRONG_TYPEDEF_INT
 *
 * ### Usage
 * @include strongly_typed_int_example.cpp
 *
 * @example strongly_typed_int_example.cpp
 */
#pragma once

#include <base/preproc/utils.hpp>

#include <type_traits>  // IWYU pragma: export

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(op, ret_type)                   \
	inline constexpr ret_type operator op(const SELF_T& rhs) const noexcept { \
		return ret_type(value op rhs.value);                                  \
	}

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(op, ret_type)            \
	inline constexpr ret_type operator op(const BASE_T& rhs) const noexcept { \
		return ret_type(value op rhs);                                        \
	}

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(op, inner_op)     \
	inline constexpr SELF_T& operator op(const SELF_T & rhs) noexcept { \
		value = static_cast<BASE_T>(value inner_op rhs.value);          \
		return *this;                                                   \
	}

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(op, inner_op) \
	inline constexpr SELF_T& operator op(const BASE_T & rhs) noexcept {    \
		value = static_cast<BASE_T>(value inner_op rhs);                   \
		return *this;                                                      \
	}


#define STRONG_TYPEDEF_INT_AUX(NAME, BASE, EXPLICIT_BASE, DIMENSIONAL)                             \
	class NAME final {                                                                             \
	private:                                                                                       \
		using BASE_T = BASE;                                                                       \
		using SELF_T = NAME;                                                                       \
		BASE value;                                                                                \
                                                                                                   \
	public:                                                                                        \
		inline NAME()                               = default;                                     \
		inline NAME(const NAME& mX)                 = default;                                     \
		inline NAME(NAME&& mX) noexcept             = default;                                     \
		inline NAME& operator=(const NAME& rhs)     = default;                                     \
		inline NAME& operator=(NAME&& rhs) noexcept = default;                                     \
		inline constexpr explicit(EXPLICIT_BASE) NAME(const BASE& x) noexcept: value{ x } {}       \
		template<typename T>                                                                       \
		requires(std::is_arithmetic_v<T>)                                                          \
		inline constexpr explicit(true) NAME(T x) noexcept: value(static_cast<BASE>(x)) {}         \
		inline constexpr explicit(EXPLICIT_BASE) operator BASE() const noexcept { return value; }  \
		inline constexpr explicit(EXPLICIT_BASE) operator BASE() noexcept { return value; }        \
		template<typename T>                                                                       \
		inline constexpr explicit(true) operator T() const noexcept {                              \
			return static_cast<T>(value);                                                          \
		}                                                                                          \
		template<typename T = BASE_T>                                                              \
		inline constexpr T asInt() const noexcept {                                                \
			return T(value);                                                                       \
		}                                                                                          \
		inline constexpr NAME operator+() const noexcept { return NAME(+value); }                  \
		inline constexpr NAME operator-() const noexcept {                                         \
			return NAME(static_cast<BASE_T>(-value));                                              \
		}                                                                                          \
		inline constexpr NAME& operator++() noexcept {                                             \
			value++;                                                                               \
			return *this;                                                                          \
		}                                                                                          \
		inline constexpr NAME& operator--() noexcept {                                             \
			value--;                                                                               \
			return *this;                                                                          \
		}                                                                                          \
		inline constexpr NAME operator++(int) noexcept {                                           \
			NAME old = *this;                                                                      \
			value++;                                                                               \
			return old;                                                                            \
		}                                                                                          \
		inline constexpr NAME operator--(int) noexcept {                                           \
			NAME old = *this;                                                                      \
			value--;                                                                               \
			return old;                                                                            \
		}                                                                                          \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(==, bool)                                            \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(!=, bool)                                            \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(<, bool)                                             \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(>, bool)                                             \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(<=, bool)                                            \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(>=, bool)                                            \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(+, SELF_T)                                           \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(-, SELF_T)                                           \
		STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(+=, +)                                       \
		STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(-=, -)                                       \
		IF_THEN_ELSE(DIMENSIONAL,                                                                            \
		   STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(*, SELF_T)                                 \
		       STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(/, SELF_T)                             \
		           STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(*=, *)                     \
		               STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(/=, /),                \
		   STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(*, SELF_T)                                        \
		       STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(/, SELF_T)                                    \
		           STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(%, SELF_T)                                \
		               STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(*=, *)                        \
		                   STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(/=, /)                    \
		                       STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(%=, %))               \
	};                                                                                             \
                                                                                                   \
	static_assert(                                                                                 \
		std::is_integral_v<BASE>,                                                                  \
		"STRONG_TYPEDEF_INT can only define integral types. Use `STRONG_TYPEDEF` for any generic " \
		"types"                                                                                    \
	)

/**
 * @brief This macro is intended to create strongly typed
 * numeric types that are dimensional, ex kg, m, bytes.
 * Usage: STRONG_TYPEDEF_INT_DIMENSIONAL(Meters, i64), creates new type Meters,
 * that behaves exactly like i64 but can only be explicitly cast to it.
 *
 * Allows for operations like 2kg * 2, but not for 2kg*2kg.
 */
#define STRONG_TYPEDEF_INT_DIMENSIONAL(NAME, BASE) STRONG_TYPEDEF_INT_AUX(NAME, BASE, true, true)

/**
 * @brief This macro is intended to create strongly typed
 * numeric types that are dimensionless, ex better ints.
 * Usage: STRONG_TYPEDEF_INT(MyOwnI32, i32), creates new type MyOwnI32,
 * that behaves exactly like i32 but can only be explicitly cast to it.
 *
 * Allows for operations like MyOwnI32 * MyOwnI32
 */
#define STRONG_TYPEDEF_INT(NAME, BASE) STRONG_TYPEDEF_INT_AUX(NAME, BASE, true, false)

#define STRONGLY_TYPED_INT_STD_HASH(TYPE)                                                  \
	template<>                                                                             \
	struct std::hash<TYPE> final {                                                         \
		usize operator()(const TYPE& id) const noexcept { return static_cast<usize>(id); } \
	};
