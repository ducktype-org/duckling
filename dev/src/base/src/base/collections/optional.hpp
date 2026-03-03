/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief ``base::Optional`` is our wrapper around ``std::optional``.
 *
 * ### Usage:
 * @code
    base::Optional<int> opt(4);
    match_optional(opt) {
        opt_some(val) {
            // Opt's value is now accessible through val!
            std::cout << "Value: " << val << '\n';
        }
        opt_none { std::cout << "No value!\n"; }
    }

    // Or simply
    base::Optional<int> magic_number = 42;
    if_opt_some(magic_number, value) { std::cout << "Magic number = " << value << "\n"; }

    if_opt_none(magic_number) { std::cout << "magic_number holds no value.\n"; }
 * @endcode
 *
 * Output:
 * @code
    Value: 4
    Magic number = 42
 * @endcode
 *
 * @example optional_simple_example.cpp
 */
#pragma once

#include <base/except/exceptions.hpp>
#include <base/preproc/diagnostics.hpp>

#include <functional>
#include <optional>
#include <utility>

/* Some cool macros.
 *
 * Example use:
 *
 * base::Optional<int> test(4);
 * match_optional(test) {
 * 	 opt_some(val) {
 * 	   std::cout << "Value: " << val << '\n';
 * 	 }
 * 	 opt_none {
 * 	   std::cout << "No value!\n";
 * 	 }
 * }
 *
 * // Or simply
 * base::optional<int> test2 = 42;
 * if_opt_some(test2, value) {
 * 	std::cout << "Test2 has value of: " << value << "\n";
 * }
 *
 * if_opt_none(test2) {
 * 	std::cout << "Test2 has no value.\n";
 * }
 *
 *
 * // Furthermore, it can be used with `std::expected<T, K>`!
 *	std::expected<int, float> t = 1;
 *	match_optional(t) {
 *		opt_some(val) { assert(val == 1); }
 *		opt_err(err) { CORE_PANIC("No error!") }
 *	}
 *  t = std::unexpected(1.5f);
 *	match_optional(t) {
 *		opt_some(val) { CORE_PANIC("No value!") }
 *		opt_err(err) { assert(err == 1.5f); }
 *	}
 *
 */
#define match_optional(optional)                                                             \
	PUSH_DIAGNOSTIC                                                                          \
	NO_SHADOW                                                                                \
	if (bool _perform_match = true)                                                          \
		for (auto&& _internal_optional = (optional); _perform_match; _perform_match = false) \
	POP_DIAGNOSTIC

#define opt_some(_value_name)                                                            \
	PUSH_DIAGNOSTIC                                                                      \
	NO_SHADOW                                                                            \
	if (bool _perform_if = _internal_optional.has_value())                               \
		for (auto&& _value_name = *_internal_optional; _perform_if; _perform_if = false) \
	POP_DIAGNOSTIC

#define opt_some_move(_value_name)                                                                  \
	PUSH_DIAGNOSTIC                                                                                 \
	NO_SHADOW                                                                                       \
	if (bool _perform_if = _internal_optional.has_value())                                          \
		for (auto&& _value_name = *std::move(_internal_optional); _perform_if; _perform_if = false) \
	POP_DIAGNOSTIC

#define opt_err(_err_name)                                                                    \
	PUSH_DIAGNOSTIC                                                                           \
	NO_SHADOW                                                                                 \
	if (bool _perform_if = !_internal_optional.has_value())                                   \
		for (auto&& _err_name = _internal_optional.error(); _perform_if; _perform_if = false) \
	POP_DIAGNOSTIC

#define opt_err_move(_err_name)                                                     \
	PUSH_DIAGNOSTIC                                                                 \
	NO_SHADOW                                                                       \
	if (bool _perform_if = !_internal_optional.has_value())                         \
		for (auto&& _err_name = std::move(_internal_optional).error(); _perform_if; \
		     _perform_if      = false)                                              \
	POP_DIAGNOSTIC

#define opt_none    \
	PUSH_DIAGNOSTIC \
	NO_SHADOW       \
	if (!_internal_optional.has_value()) POP_DIAGNOSTIC

#define if_opt_some(optional, _value_name)                                    \
	PUSH_DIAGNOSTIC                                                           \
	NO_SHADOW                                                                 \
	if (auto&& _internal_optional = (optional))                               \
		if (bool _if_opt_some_stop = true)                                    \
			for (auto&& _value_name = *_internal_optional; _if_opt_some_stop; \
			     _if_opt_some_stop  = false)                                  \
	POP_DIAGNOSTIC


#define if_opt_none(optional) \
	PUSH_DIAGNOSTIC           \
	NO_SHADOW                 \
	if (!optional.has_value()) POP_DIAGNOSTIC

namespace base {

	template<class T>
	class Ref;

	template<class T>
	using CRef = Ref<const T>;

	/**
	 * base::Optional is analogous to std::optional, but better.
	 * As its name naturally suggests,
	 * it may or may not hold a value underneath,
	 * and the programmer has to first
	 * check for it's presence.
	 *
	 * Macros defined above may come in handy when dealing with these creatures.
	 * Also methods - map and flatMap - are very useful.
	 *
	 * base::Optional does not inherit from std::optional, because std::optional doesn't throw on
	 * null-value access, but base::optional does.
	 *
	 * @tparam T type of stored value. It can be a value or a Ref<T> for non-owning references.
	 *
	 * For non-owning references, use base::Optional<Ref<T>> instead of base::Optional<T&>.
	 * Example:
	 *   A a;
	 *   base::Optional<Ref<A>> opt = &a;
	 *   if (opt) { ... }
	 */
	template<class T>
	requires(!std::is_reference_v<T>) class Optional final {
		std::optional<T> private_optional;

		/**
		 * Const and reference qualified type of value in line with qualifications of the
		 * surrounding optional.
		 */
		template<class Self>
		using QualifiedT = decltype(std::declval<Self>().private_optional.value());

		constexpr void throwOnNoValue() const {
			if (!has_value()) CORE_PANIC("Tried to retrieve a value from an empty optional.");
		}

	public:
		Optional()  = default;
		~Optional() = default;

		Optional(std::nullopt_t) noexcept {}

		Optional(Optional&&)                 = default;
		Optional(const Optional&)            = default;
		Optional& operator=(Optional&&)      = default;
		Optional& operator=(const Optional&) = default;

		Optional(const T& value
		) noexcept(std::is_nothrow_constructible_v<std::optional<T>, const T&>):
			  private_optional(value) {}

		template<class... Args>
		requires(sizeof...(Args) >= 2) && std::is_constructible_v<T, Args...>
		constexpr explicit Optional(Args&&... args
		) noexcept(std::is_nothrow_constructible_v<T, Args...>):
			  private_optional(std::make_optional<T>(std::forward<Args>(args)...)) {}

		template<class U = T>
		requires(!std::is_same_v<std::remove_cvref_t<U>, Optional>) && std::is_constructible_v<T, U>
		constexpr Optional(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>):
			  private_optional(std::forward<U>(value)) {}

		template<class... Args>
		constexpr T& emplace(Args&&... args
		) noexcept(std::is_nothrow_constructible_v<T, Args&&...>) {
			return private_optional.emplace(std::forward<Args>(args)...);
		}

		explicit constexpr operator bool() const noexcept { return has_value(); }

		template<class U = T>
		requires(!std::is_same_v<std::remove_cvref_t<U>, Optional>) && std::is_constructible_v<T, U>
		constexpr Optional& operator=(U&& value
		) noexcept(std::is_nothrow_constructible_v<T, U&&> && std::is_nothrow_assignable_v<T, U&&>) {
			private_optional = std::forward<U>(value);
			return *this;
		}

		constexpr void reset() noexcept { private_optional.reset(); }

		friend void swap(Optional& a, Optional& b) noexcept {
			std::swap(a.private_optional, b.private_optional);
		}

		/**
		 * Check if base::Optional hold a value.
		 * @return True if holds, false otherwise
		 */
		[[nodiscard]]
		// NOLINTBEGIN(readability-identifier-naming)
		// Leaving matching name of the method so it matches std::expected's
		// and our monadic macros work for both.
		constexpr bool has_value() const noexcept {
			// NOLINTEND(readability-identifier-naming)
			return private_optional.has_value();
		}

		/**
		 * Check if base::Optional is empty.
		 * @return True if empty, false otherwise
		 */
		[[nodiscard]]
		constexpr bool empty() const noexcept {
			return !has_value();
		}

		// Accessors.
		/**
		 * Get value from base::Optional.
		 * @throws Throws on no value.
		 * @return Value that it holds.
		 */
		template<class Self>
		[[nodiscard]]
		constexpr QualifiedT<Self> value(this Self&& self) {
			self.throwOnNoValue();
			return std::forward<Self>(self).private_optional.value();
		}

		/**
		 * Get the stored value or a given backup.
		 * @param or_value value to be returned if empty.
		 * @details Notice that this function returns by value, which may lead to unwanted copies.
		 * @return Stored value if exists, otherwise or_value.
		 */
		template<class Self, class U = T>
		[[nodiscard]]
		constexpr T copyValueOr(this Self&& self, U&& or_value) {
			if (self.has_value())
				return std::forward<Self>(self).value();
			else
				return static_cast<T>(std::forward<U>(or_value));
		}

		/**
		 * Like *ptr - returns an object stored underneath.
		 * @throws Throws on no value.
		 * @return stored object
		 */
		template<class Self>
		[[nodiscard]]
		constexpr QualifiedT<Self> operator*(this Self&& self) {
			return std::forward<Self>(self).value();
		}

		/**
		 * Get value or CORE_PANIC with message.
		 * @param message message to be passed to the panic
		 * @return
		 */
		template<class Self>
		[[nodiscard]]
		constexpr QualifiedT<Self> expect(
			this Self&& self, [[maybe_unused]] std::string_view message
		) {
			if (!self.has_value()) CORE_PANIC(message);
			return std::forward<Self>(self).value();
		}

		/**
		 * Get value or throw a given error.
		 * @tparam Err error type to be thrown
		 * @param args arguments passed to a constructor of the error type.
		 */
		template<class Err, class... Args, class Self>
		requires(std::constructible_from<Err, Args && ...>) [[nodiscard]]
		constexpr QualifiedT<Self> expect(this Self&& self, Args&&... args) {
			if (!self.has_value()) throw Err(std::forward<Args>(args)...);
			return std::forward<Self>(self).value();
		}

		/**
		 * An operator that allows a direct data access.
		 * @return Object T to perform an operation on.
		 */
		template<class Self>
		[[nodiscard]]
		constexpr auto operator->(this Self&& self) {
			self.throwOnNoValue();
			return std::forward<Self>(self).private_optional.operator->();
		}

		/**
		 * Applies the passed function on the value and wraps in base::Optional if the object
		 * contains a value, otherwise does nothing.
		 * @tparam Function
		 * @param function Function to apply on the value. Function must take one argument which
		 * type has to match the optional's type (auto works too). Function can return any type of
		 * data.
		 * @return If object contains a value, then applies a function, otherwise does nothing.
		 */
		template<class Function, class Self>
		requires std::invocable<Function&&, QualifiedT<Self>>
		constexpr auto map(this Self&& self, Function&& function)
			-> Optional<std::invoke_result_t<Function, QualifiedT<Self>>> {
			if (self.has_value()) {
				return std::invoke(
					std::forward<Function>(function), std::forward<Self>(self).value()
				);
			} else {
				return {};
			}
		}

		/**
		 * Idea of a flatMap is simple: If "function" returns Optional<X>, then we use a flat map,
		 * and we don't end up with Optional<Optional<X>>, but Optional<X>.
		 * @tparam Function
		 * @param function Function to apply on the value. Function must take one argument which
		 * type has to match the optional's type (auto works too), and should return Optional<U>;
		 * @return If object contains a value, then applies a function, otherwise does nothing.
		 */
		template<class Function, class Self>
		requires std::invocable<Function&&, QualifiedT<Self>>
		constexpr auto flatMap(this Self&& self, Function&& function)
			-> std::invoke_result_t<Function, QualifiedT<Self>> {
			using result_type = std::invoke_result_t<Function&&, QualifiedT<Self>>;
			static_assert(IsOfSameClass<result_type, Optional>);
			if (self.has_value()) {
				return std::invoke(
					std::forward<Function>(function), std::forward<Self>(self).value()
				);
			} else {
				return {};
			}
		}
	};

	template<class U, class T>
	constexpr bool operator==(const Optional<U>& one, const Optional<T>& other) {
		return (one.has_value() && other.has_value() && one.value() == other.value())
		    || (!one.has_value() && !other.has_value());
	}

	template<class U, class T>
	constexpr bool operator!=(const Optional<U>& one, const Optional<T>& other) {
		return !(one == other);
	}

	template<class U, class T>
	constexpr bool operator<(const Optional<U>& one, const Optional<T>& other) {
		return (one.has_value() && other.has_value() && one.value() < other.value())
		    || (!one.has_value() && other.has_value());
	}

	template<class U, class T>
	constexpr bool operator>(const Optional<U>& one, const Optional<T>& other) {
		return (one.has_value() && other.has_value() && one.value() > other.value())
		    || (one.has_value() && !other.has_value());
	}

	template<class U, class T>
	constexpr bool operator<=(const Optional<U>& one, const Optional<T>& other) {
		return (one.has_value() && other.has_value() && one.value() <= other.value())
		    || other.has_value();
	}

	template<class U, class T>
	constexpr bool operator>=(const Optional<U>& one, const Optional<T>& other) {
		return (one.has_value() && other.has_value() && one.value() >= other.value())
		    || one.has_value();
	}

	template<class T>
	constexpr std::strong_ordering operator<=>(Optional<T>& obj, std::nullopt_t) {
		if (obj.has_value())
			return std::strong_ordering::greater;
		else
			return std::strong_ordering::equal;
	}

	template<class T>
	constexpr std::strong_ordering operator<=>(std::nullopt_t, Optional<T>& obj) {
		if (obj.has_value())
			return std::strong_ordering::less;
		else
			return std::strong_ordering::equal;
	}

	template<class U, class T>
	constexpr bool operator==(const Optional<U>& opt, const T& value) {
		return opt.has_value() && opt.value() == value;
	}

	template<class U, class T>
	constexpr bool operator==(const T& value, const Optional<U>& opt) {
		return opt.has_value() && opt.value() == value;
	}

	template<class U, class T>
	constexpr bool operator!=(const Optional<U>& opt, const T& value) {
		return !(opt == value);
	}

	template<class U, class T>
	constexpr bool operator!=(const T& value, const Optional<U>& opt) {
		return !(opt == value);
	}

	template<class U, class T>
	constexpr bool operator<(const Optional<U>& opt, const T& value) {
		return !opt.has_value() || opt.value() < value;
	}

	template<class U, class T>
	constexpr bool operator<(const T& value, const Optional<U>& opt) {
		return opt.has_value() && value < opt.value();
	}

	template<class U, class T>
	constexpr bool operator>(const Optional<U>& opt, const T& value) {
		return opt.has_value() && value < opt.value();
	}

	template<class U, class T>
	constexpr bool operator>(const T& value, const Optional<U>& opt) {
		return !opt.has_value() || opt.value() < value;
	}

	template<class U, class T>
	constexpr bool operator<=(const Optional<U>& opt, const T& value) {
		return !opt.has_value() || opt.value() <= value;
	}

	template<class U, class T>
	constexpr bool operator<=(const T& value, const Optional<U>& opt) {
		return opt.has_value() && value <= opt.value();
	}

	template<class U, class T>
	constexpr bool operator>=(const Optional<U>& opt, const T& value) {
		return opt.has_value() && value <= opt.value();
	}

	template<class U, class T>
	constexpr bool operator>=(const T& value, const Optional<U>& opt) {
		return !opt.has_value() || opt.value() <= value;
	}

}  // base
