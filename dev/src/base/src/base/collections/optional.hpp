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

#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/preproc/diagnostics.hpp>

#include <compare>
#include <concepts>
#include <expected>
#include <functional>
#include <optional>
#include <type_traits>
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
 * // For that case `match_expected` in `base/collections/expected.hpp` reads better, but both work.
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
#define match_optional(optional) \
	PUSH_DIAGNOSTIC              \
	NO_SHADOW                    \
	if (auto&& _internal_optional = (optional); true) POP_DIAGNOSTIC

#define opt_some(_value_name)           \
	PUSH_DIAGNOSTIC                     \
	NO_SHADOW                           \
	if (_internal_optional.has_value()) \
		if (auto&& _value_name = *_internal_optional; true) POP_DIAGNOSTIC

#define opt_some_move(_value_name)      \
	PUSH_DIAGNOSTIC                     \
	NO_SHADOW                           \
	if (_internal_optional.has_value()) \
		if (auto&& _value_name = *std::move(_internal_optional); true) POP_DIAGNOSTIC

#define opt_err(_err_name)               \
	PUSH_DIAGNOSTIC                      \
	NO_SHADOW                            \
	if (!_internal_optional.has_value()) \
		if (auto&& _err_name = _internal_optional.error(); true) POP_DIAGNOSTIC

#define opt_err_move(_err_name)          \
	PUSH_DIAGNOSTIC                      \
	NO_SHADOW                            \
	if (!_internal_optional.has_value()) \
		if (auto&& _err_name = std::move(_internal_optional).error(); true) POP_DIAGNOSTIC

#define opt_none    \
	PUSH_DIAGNOSTIC \
	NO_SHADOW       \
	if (!_internal_optional.has_value()) POP_DIAGNOSTIC

#define if_opt_some(optional, _value_name)      \
	PUSH_DIAGNOSTIC                             \
	NO_SHADOW                                   \
	if (auto&& _internal_optional = (optional)) \
		if (auto&& _value_name = *_internal_optional; true) POP_DIAGNOSTIC


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
	 * The rest of the interface mirrors Rust's Option: copyValueOr / copyValueOrElse /
	 * copyValueOrDefault to fall back on emptiness, mapOr / mapOrElse / filter / flatten / inspect
	 * / orElse to keep transforming, okOr to turn emptiness into a std::expected error, and take /
	 * replace / getOrInsert to change what is stored. Every "OrElse" flavour takes a callable and
	 * runs it only when the optional is empty, so an expensive fallback costs nothing when there is
	 * a value. Just like in Rust, that callable is always called with no arguments: capture what it
	 * needs in the lambda. The methods that do take arguments - emplace, replace, getOrInsert,
	 * expect and okOr - forward them to a constructor, they never call anything.
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

		/**
		 * @brief Move the stored value out, leaving this optional empty.
		 * @return An optional with the value that was stored, or an empty one if there was none.
		 */
		[[nodiscard]]
		constexpr Optional take() noexcept(std::is_nothrow_move_constructible_v<T>) {
			Optional taken = std::move(*this);
			reset();
			return taken;
		}

		/**
		 * @brief Store a new value built in place, and give back whatever was stored before.
		 * @param args arguments passed to a constructor of T.
		 * @return An optional with the previous value, or an empty one if there was none.
		 */
		template<class... Args>
		requires std::is_constructible_v<T, Args&&...> constexpr Optional replace(Args&&... args) {
			Optional previous = take();
			emplace(std::forward<Args>(args)...);
			return previous;
		}

		/**
		 * @brief Get a reference to the stored value, building it first if the optional is empty.
		 * @param args arguments passed to a constructor of T. They are used only when empty.
		 * @return Reference to the stored value. It never panics, because the optional always holds
		 * a value once this returns.
		 */
		template<class... Args>
		requires std::is_constructible_v<T, Args&&...> constexpr T& getOrInsert(Args&&... args) {
			if (!has_value()) emplace(std::forward<Args>(args)...);
			return private_optional.value();
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
		 * @brief Get the stored value, or the result of calling a fallback function.
		 * @param function called with no arguments, and only when the optional is empty.
		 * @details Unlike copyValueOr, the fallback is not built unless it is really needed, so an
		 * expensive default costs nothing on a non-empty optional.
		 * @return Stored value if exists, otherwise function().
		 */
		template<class Self, class Function>
		requires std::invocable<Function&&>
		      && std::convertible_to<std::invoke_result_t<Function&&>, T> [[nodiscard]]
		constexpr T copyValueOrElse(this Self&& self, Function&& function) {
			if (self.has_value()) return std::forward<Self>(self).value();
			return static_cast<T>(std::invoke(std::forward<Function>(function)));
		}

		/**
		 * @brief Get the stored value, or a default constructed one.
		 * @details Only available when T can be default constructed.
		 * @return Stored value if exists, otherwise T{}.
		 */
		template<class Self>
		requires std::default_initializable<T> [[nodiscard]]
		constexpr T copyValueOrDefault(this Self&& self) {
			if (self.has_value()) return std::forward<Self>(self).value();
			return T{};
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
			-> Optional<std::remove_cvref_t<std::invoke_result_t<Function&&, QualifiedT<Self>>>> {
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

		/**
		 * @brief Applies the passed function on the value, or returns a given backup.
		 * @param or_value returned when the optional is empty.
		 * @param function applied on the value when there is one.
		 * @details This is map() followed by copyValueOr(), without the intermediate optional.
		 * @return function(value) if there is a value, otherwise or_value.
		 */
		template<class Self, class Function, class U>
		requires std::invocable<Function&&, QualifiedT<Self>>
		constexpr auto mapOr(this Self&& self, U&& or_value, Function&& function)
			-> std::remove_cvref_t<std::invoke_result_t<Function&&, QualifiedT<Self>>> {
			using result_type
				= std::remove_cvref_t<std::invoke_result_t<Function&&, QualifiedT<Self>>>;
			if (self.has_value()) {
				return std::invoke(
					std::forward<Function>(function), std::forward<Self>(self).value()
				);
			} else {
				return static_cast<result_type>(std::forward<U>(or_value));
			}
		}

		/**
		 * @brief Applies the passed function on the value, or calls a fallback function.
		 * @param or_function called with no arguments when the optional is empty.
		 * @param function applied on the value when there is one.
		 * @details Only the branch that is actually taken runs, so neither side pays for the other.
		 * @return function(value) if there is a value, otherwise or_function().
		 */
		template<class Self, class OrFunction, class Function>
		requires std::invocable<Function&&, QualifiedT<Self>> && std::invocable<OrFunction&&>
		constexpr auto mapOrElse(this Self&& self, OrFunction&& or_function, Function&& function)
			-> std::remove_cvref_t<std::invoke_result_t<Function&&, QualifiedT<Self>>> {
			using result_type
				= std::remove_cvref_t<std::invoke_result_t<Function&&, QualifiedT<Self>>>;
			if (self.has_value()) {
				return std::invoke(
					std::forward<Function>(function), std::forward<Self>(self).value()
				);
			} else {
				return static_cast<result_type>(std::invoke(std::forward<OrFunction>(or_function)));
			}
		}

		/**
		 * @brief Keeps the value only when it satisfies a predicate.
		 * @param predicate called with a reference to the stored value.
		 * @return A copy of this optional when it holds a value the predicate accepts, an empty
		 * optional in every other case.
		 */
		template<class Self, class Predicate>
		requires std::predicate<Predicate&&, QualifiedT<Self&>> [[nodiscard]]
		constexpr Optional filter(this Self&& self, Predicate&& predicate) {
			if (self.has_value() && std::invoke(std::forward<Predicate>(predicate), self.value()))
				return std::forward<Self>(self).value();
			return {};
		}

		/**
		 * @brief Returns this optional when it holds a value, otherwise the one a function makes.
		 * @param function called with no arguments, and only when the optional is empty. It has to
		 * return an optional of the same type.
		 * @return This optional if it is not empty, otherwise function().
		 */
		template<class Self, class Function>
		requires std::invocable<Function&&> [[nodiscard]]
		constexpr Optional orElse(this Self&& self, Function&& function) {
			using result_type = std::invoke_result_t<Function&&>;
			static_assert(std::same_as<std::remove_cvref_t<result_type>, Optional>);
			if (self.has_value()) return std::forward<Self>(self).value();
			return std::invoke(std::forward<Function>(function));
		}

		/**
		 * @brief Calls a function on the stored value, if any, and hands the optional back.
		 * @param function called with a reference to the stored value. Its result is ignored.
		 * @details The value is only observed, never moved out, so chaining stays safe:
		 * optional.inspect(log).map(convert).
		 * @return The very same optional, so calls can be chained.
		 */
		template<class Self, class Function>
		requires std::invocable<Function&&, QualifiedT<Self&>>
		constexpr Self&& inspect(this Self&& self, Function&& function) {
			if (self.has_value()) std::invoke(std::forward<Function>(function), self.value());
			return std::forward<Self>(self);
		}

		/**
		 * @brief Removes one level of nesting from an Optional<Optional<U>>.
		 * @details Only available when the stored type is itself a base::Optional.
		 * @return The inner optional, or an empty one when the outer optional is empty.
		 */
		template<class Self>
		requires IsOfSameClass<T, Optional> [[nodiscard]]
		constexpr T flatten(this Self&& self) {
			if (self.has_value()) return std::forward<Self>(self).value();
			return {};
		}

		/**
		 * @brief Turns the optional into a std::expected, so an empty one becomes an error.
		 * @tparam Err error type stored when the optional is empty.
		 * @param args arguments passed to a constructor of the error type.
		 * @details The result can be matched with match_expected, from
		 * base/collections/expected.hpp.
		 * @return std::expected holding the value, or Err(args...) when the optional is empty.
		 */
		template<class Err, class... Args, class Self>
		requires(std::constructible_from<Err, Args && ...>) [[nodiscard]]
		constexpr std::expected<T, Err> okOr(this Self&& self, Args&&... args) {
			if (self.has_value()) return std::forward<Self>(self).value();
			return std::unexpected<Err>(std::forward<Args>(args)...);
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

	// An empty Optional orders before any engaged one, matching the relational operators above.
	template<class U, class T>
	constexpr auto operator<=>(const Optional<U>& one, const Optional<T>& other)
		-> decltype(one.value() <=> other.value()) {
		if (one.has_value() && other.has_value()) return one.value() <=> other.value();
		return static_cast<decltype(one.value() <=> other.value())>(
			one.has_value() <=> other.has_value()
		);
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
