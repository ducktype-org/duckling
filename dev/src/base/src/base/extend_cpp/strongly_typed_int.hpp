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

#include <base/preproc/macro_base.hpp>

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

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_BINARY_OPERATIONS_AUX             \
	inline constexpr SELF_T operator~() const noexcept {          \
		return SELF_T(static_cast<BASE_T>(~value));               \
	}                                                             \
	STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(&, SELF_T)              \
	STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(|, SELF_T)              \
	STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(^, SELF_T)              \
	STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(<<, SELF_T)             \
	STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(>>, SELF_T)             \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(&=, &)          \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(|=, |)          \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(^=, ^)          \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(<<=, <<)        \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(>>=, >>)        \
	STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(<<, SELF_T)      \
	STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(>>, SELF_T)      \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(<<=, <<) \
	STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(>>=, >>)


#define STRONG_TYPEDEF_INT_AUX(NAME, BASE, EXPLICIT_BASE, DIMENSIONAL, BINARY)                     \
	class NAME final {                                                                             \
	private:                                                                                       \
		using BASE_T = BASE;                                                                       \
		using SELF_T = NAME;                                                                       \
		BASE value;                                                                                \
                                                                                                   \
	public:                                                                                        \
		NAME()                           = default;                                                \
		NAME(const NAME&)                = default;                                                \
		NAME(NAME&&) noexcept            = default;                                                \
		NAME& operator=(const NAME&)     = default;                                                \
		NAME& operator=(NAME&&) noexcept = default;                                                \
		constexpr explicit(EXPLICIT_BASE) NAME(const BASE& x) noexcept: value{ x } {}              \
		template<typename T>                                                                       \
		requires(std::is_arithmetic_v<T>)                                                          \
		constexpr explicit(true) NAME(T x) noexcept: value(static_cast<BASE>(x)) {}                \
		constexpr explicit(EXPLICIT_BASE) operator BASE() const noexcept { return value; }         \
		constexpr explicit(EXPLICIT_BASE) operator BASE() noexcept { return value; }               \
		template<typename T>                                                                       \
		constexpr explicit(true) operator T() const noexcept {                                     \
			return static_cast<T>(value);                                                          \
		}                                                                                          \
		template<typename T = BASE_T>                                                              \
		constexpr T asInt() const noexcept {                                                       \
			return T(value);                                                                       \
		}                                                                                          \
		constexpr NAME  operator+() const noexcept { return NAME(+value); }                        \
		constexpr NAME  operator-() const noexcept { return NAME(static_cast<BASE_T>(-value)); }   \
		constexpr NAME& operator++() noexcept {                                                    \
			value++;                                                                               \
			return *this;                                                                          \
		}                                                                                          \
		constexpr NAME& operator--() noexcept {                                                    \
			value--;                                                                               \
			return *this;                                                                          \
		}                                                                                          \
		constexpr NAME operator++(int) noexcept {                                                  \
			NAME old = *this;                                                                      \
			value++;                                                                               \
			return old;                                                                            \
		}                                                                                          \
		constexpr NAME operator--(int) noexcept {                                                  \
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
		IF(DIMENSIONAL)(                                                                           \
			STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(*, SELF_T)                                \
				STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(/, SELF_T)                            \
					STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(*=, *)                    \
						STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(/=, /),               \
			STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(*, SELF_T)                                       \
				STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(/, SELF_T)                                   \
					STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(%, SELF_T)                               \
						STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(*=, *)                       \
							STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(/=, /)                   \
								STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(%=, %)               \
		) WHEN(BINARY)(STRONG_TYPEDEF_INT_MAKE_BINARY_OPERATIONS_AUX)                              \
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
#define STRONG_TYPEDEF_INT_DIMENSIONAL(NAME, BASE) STRONG_TYPEDEF_INT_AUX(NAME, BASE, true, 1, 0)

/**
 * @brief This macro is intended to create strongly typed
 * numeric types that are dimensionless, ex better ints.
 * Usage: STRONG_TYPEDEF_INT(MyOwnI32, i32), creates new type MyOwnI32,
 * that behaves exactly like i32 but can only be explicitly cast to it.
 *
 * Allows for operations like MyOwnI32 * MyOwnI32
 */
#define STRONG_TYPEDEF_INT(NAME, BASE) STRONG_TYPEDEF_INT_AUX(NAME, BASE, true, 0, 1)

#define STRONGLY_TYPED_INT_STD_HASH(TYPE)                                                  \
	template<>                                                                             \
	struct std::hash<TYPE> final {                                                         \
		usize operator()(const TYPE& id) const noexcept { return static_cast<usize>(id); } \
	};
