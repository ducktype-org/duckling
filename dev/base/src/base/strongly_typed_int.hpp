#pragma once

#include <type_traits>

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(op, ret_type) \
inline constexpr ret_type operator op (const SELF_T& rhs) const noexcept {  \
	return ret_type(value op rhs.value);                                    \
}

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(op, ret_type) \
inline constexpr ret_type operator op (const BASE_T& rhs) const noexcept {  \
	return ret_type(value op rhs);                                          \
}

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(op) \
inline constexpr SELF_T& operator op (const SELF_T& rhs) noexcept {   \
	value op rhs.value;                                               \
	return *this;                                                     \
}

/**
 * @brief This is helper macro, do not use directly
 */
#define STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(op) \
inline constexpr SELF_T& operator op (const BASE_T& rhs) noexcept {   \
	value op rhs;                                                     \
	return *this;                                                     \
}



/**
 * @brief This macro is intended to create strongly typed
 * numeric types.
 * Usage: STRONG_TYPEDEF_INT(Meters, int64_t), created new type Meters, 
 * that behave exactly like int64_t but can only by explicitly casted to it.
 */
#define STRONG_TYPEDEF_INT(NAME, BASE)                                          \
	class NAME final {                                                          \
	private:                                                                    \
		using BASE_T = BASE;													\
		using SELF_T = NAME;													\
		BASE value;                                                             \
																				\
	public:                                                                     \
		inline NAME() = default;                                                \
		inline NAME(const NAME& mX) = default;                                  \
		inline NAME(NAME&& mX) noexcept = default;                              \
		inline NAME& operator=(const NAME& rhs) = default;                      \
		inline NAME& operator=(NAME&& rhs) noexcept = default;                  \
		inline constexpr explicit NAME(const BASE& x) noexcept: value{x} {}     \
		inline constexpr explicit operator const BASE&() const noexcept {       \
			return value;                                                       \
		}                                                                       \
		inline constexpr explicit operator BASE&() noexcept { return value; }      \
		inline constexpr NAME operator+() const noexcept { return NAME(+value); }  \
		inline constexpr NAME operator-() const noexcept { return NAME(-value); }  \
		inline constexpr NAME& operator++() noexcept { value++; return *this; }  \
		inline constexpr NAME& operator--() noexcept { value--; return *this; }  \
		inline constexpr NAME operator++(int) noexcept { NAME old = *this; value++; return old; } \
		inline constexpr NAME operator--(int) noexcept { NAME old = *this; value--; return old; } \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(==, bool) \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(!=, bool) \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(<, bool)  \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(>, bool)  \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(<=, bool) \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(>=, bool) \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(+, SELF_T)  \
		STRONG_TYPEDEF_INT_MAKE_OPERATION_AUX(-, SELF_T)  \
		STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(+=) \
		STRONG_TYPEDEF_INT_MAKE_INPLACE_OPERATION_AUX(-=) \
		STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(*, SELF_T) \
		STRONG_TYPEDEF_INT_MAKE_SCALAR_OPERATION_AUX(/, SELF_T) \
		STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(*=) \
		STRONG_TYPEDEF_INT_MAKE_INPLACE_SCALAR_OPERATION_AUX(/=) \
	};                                                                          \
                                                                                \
	static_assert(std::is_integral_v<BASE>, "STRONG_TYPEDEF_INT can only define integral types. Use `STRONG_TYPEDEF` for any generic types");

