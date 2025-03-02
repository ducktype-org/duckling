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

#include <optional>
#include <functional>

#include "exceptions.hpp"

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
 */
#define match_optional(optional)                                                             \
	PUSH_DIAGNOSTIC                                                                          \
	NO_SHADOW                                                                                \
	if (bool _perform_match = true)                                                          \
		for (auto&& _internal_optional = (optional); _perform_match; _perform_match = false) \
	POP_DIAGNOSTIC

#define opt_some(_value_name)                                                                   \
	PUSH_DIAGNOSTIC                                                                             \
	NO_SHADOW                                                                                   \
	if (bool _perform_if = _internal_optional.has_value())                                      \
		for (auto&& _value_name = _internal_optional.value(); _perform_if; _perform_if = false) \
	POP_DIAGNOSTIC

#define opt_none    \
	PUSH_DIAGNOSTIC \
	NO_SHADOW       \
	if (!_internal_optional.has_value()) POP_DIAGNOSTIC

#define if_opt_some(optional, _value_name)                                           \
	PUSH_DIAGNOSTIC                                                                  \
	NO_SHADOW                                                                        \
	if (auto&& _internal_optional = (optional))                                      \
		if (bool _if_opt_some_stop = true)                                           \
			for (auto&& _value_name = _internal_optional.value(); _if_opt_some_stop; \
			     _if_opt_some_stop  = false)                                         \
	POP_DIAGNOSTIC


#define if_opt_none(optional) \
	PUSH_DIAGNOSTIC           \
	NO_SHADOW                 \
	if (!optional.has_value()) POP_DIAGNOSTIC

namespace base {
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
	 * @tparam T type of stored value. It can be a reference.
	 */
	template<class T>
	class Optional final {
	public:
		Optional()  = default;
		~Optional() = default;

		Optional(std::nullopt_t) noexcept {}

		Optional(const T& value): private_optional(value) {}

		Optional(Optional&&) noexcept            = default;
		Optional(const Optional&)                = default;
		Optional& operator=(Optional&&) noexcept = default;
		Optional& operator=(const Optional&)     = default;

		template<class... Args>
		requires std::is_constructible_v<T, Args...> constexpr explicit Optional(Args&&... args):
			  private_optional(std::make_optional<T>(std::forward<Args>(args)...)) {}

		template<class U = T>
		requires std::is_constructible_v<T, U>
		constexpr Optional(U&& value): private_optional(std::forward<U>(value)) {}

		template<class... Args>
		constexpr T& emplace(Args&&... args) {
			return private_optional.emplace(std::forward<Args>(args)...);
		}

		explicit constexpr operator bool() const { return has_value(); }

		template<class U = T>
		requires std::is_constructible_v<T, U> constexpr Optional& operator=(U&& value) {
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
		constexpr bool has_value() const {
			return private_optional.has_value();
		}

		/**
		 * Check if base::Optional is empty.
		 * @return True if empty, false otherwise
		 */
		[[nodiscard]]
		constexpr bool empty() const {
			return !has_value();
		}

		// Accessors.
		/**
		 * Get value from base::Optional. Throw on no value.
		 * @return Value that it holds.
		 */
		[[nodiscard]]
		constexpr const T& value() const& {
			_throwOnNoValue();
			return private_optional.value();
		}

		[[nodiscard]]
		constexpr const T&& value() const&& {
			_throwOnNoValue();
			return std::move(private_optional.value());
		}

		[[nodiscard]]
		constexpr T& value() & {
			_throwOnNoValue();
			return private_optional.value();
		}

		[[nodiscard]]
		constexpr T&& value() && {
			_throwOnNoValue();
			return std::move(private_optional.value());
		}

		/**
		 * Returns value or or_value depending if base::Optional is empty or not.
		 * @param or_value value to be returned if empty
		 * @return
		 */
		[[nodiscard]]
		const T& value_or(const T& or_value) const& {
			if (has_value()) return value();
			return or_value;
		}

		[[nodiscard]]
		const T&& value_or(const T&& or_value) const&& {
			if (has_value()) return std::move(value());
			return std::move(or_value);
		}

		[[nodiscard]]
		T& value_or(T& or_value) & {
			if (has_value()) return value();
			return or_value;
		}

		[[nodiscard]]
		T&& value_or(T&& or_value) && {
			if (has_value()) return std::move(value());
			return std::move(or_value);
		}

		// clang-format off
		// Turning clang-format, because it cannot format the following functions correctly.

		/**
		 * Like *ptr - returns an object stored underneath. Throws on no value.
		 * @return stored object
		 */
		[[nodiscard]]
		constexpr const T& operator*() const& {
			return value();
		}

		[[nodiscard]]
		constexpr const T&& operator*() const&& {
			return std::move(value());
		}

		[[nodiscard]]
		constexpr T& operator*() & {
			return value();
		}

		[[nodiscard]]
		constexpr T&& operator*() && {
			return std::move(value());
		}

		// clang-format on

		/**
		 * Get value or CORE_PANIC with message.
		 * @param message message to be passed to the panic
		 * @return
		 */
		[[nodiscard]]
		constexpr const T& expect(std::string_view message) const& {
			if (!has_value()) CORE_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr const T&& expect(std::string_view message) const&& {
			if (!has_value()) CORE_PANIC(message);
			return std::move(value());
		}

		[[nodiscard]]
		constexpr T& expect(std::string_view message) & {
			if (!has_value()) CORE_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr T&& expect(std::string_view message) && {
			if (!has_value()) CORE_PANIC(message);
			return std::move(value());
		}

		/**
		 * An operator that allows a direct data access.
		 * @return Object T to perform an operation on.
		 */
		[[nodiscard]]
		constexpr const T* operator->() const {
			_throwOnNoValue();
			return private_optional.operator->();
		}

		[[nodiscard]]
		constexpr T* operator->() {
			_throwOnNoValue();
			return private_optional.operator->();
		}

		/**
		 * Applies the passed function on the value and wraps in base::Optional if the object
		 * contains a value, otherwise does nothing.
		 * @tparam Function
		 * @param function Function to apply on the value. Function must take one argument which
		 * type has to match the optional's type (auto works too). Function can return any type of
		 * data.
		 * @return On value: Optional(function(value)), otherwise does
		 * nothing.
		 */
		template<typename Function>
		auto map(const Function& function) const -> Optional<std::invoke_result_t<Function, T>> {
			if (has_value()) return function(value());
			return {};
		}

		// A non-const version.
		template<typename Function>
		auto map(const Function& function) -> Optional<std::invoke_result_t<Function, T>> {
			if (has_value()) return function(value());
			return {};
		}

		/**
		 * Idea of a flatMap is simple: If "function" returns Optional<X>, then we use a flat map,
		 * and we don't end up with Optional<Optional<X>>, but Optional<X>.
		 * @tparam Function
		 * @param function Function to apply on the value. Function must take one argument which
		 * type has to match the optional's type (auto works too), and should return Optional<U>;
		 * @return If object contains a value, then applies a function, otherwise does nothing.
		 */
		template<typename Function>
		auto flatMap(const Function& function) const -> std::invoke_result_t<Function, T> {
			static_assert(IsOfSameClass<std::invoke_result_t<Function, T>, Optional>);
			if (has_value()) return function(value());
			return {};
		}

		// A non-const version.
		template<typename Function>
		auto flatMap(const Function& function) -> std::invoke_result_t<Function, T> {
			static_assert(IsOfSameClass<std::invoke_result_t<Function, T>, Optional>);
			if (has_value()) return function(value());
			return {};
		}

	protected:
		void _throwOnNoValue() const {
			if (!has_value()) CORE_PANIC("Tried to retrieve a value from an empty optional.");
		}

	private:
		std::optional<T> private_optional;
	};

	// @WARNING: This is almost an exact copy of the code above and there is pretty much nothing
	// we can do to avoid doing it this way, mainly because std::optional<T> is not supported for
	// T being an incomplete type.
	template<class T>
	class Optional<T&> final {
	public:
		Optional() = default;

		Optional(T& value): private_optional(std::ref(value)) {}

		Optional(Optional&&) noexcept            = default;
		Optional(const Optional&)                = default;
		Optional& operator=(Optional&&) noexcept = default;
		Optional& operator=(const Optional&)     = default;

		Optional& operator=(T&& other) {
			private_optional = std::move(other);
			return *this;
		}

		Optional& operator=(T& other) {
			private_optional = std::ref(other);
			return *this;
		}

		constexpr void reset() noexcept { private_optional.reset(); }

		explicit constexpr operator bool() const { return has_value(); }

		friend void swap(Optional& a, Optional& b) noexcept {
			std::swap(a.private_optional, b.private_optional);
		}

		[[nodiscard]]
		constexpr bool has_value() const {
			return private_optional.has_value();
		}

		[[nodiscard]]
		constexpr bool empty() const {
			return !has_value();
		}

		// Accessors.
		[[nodiscard]]
		constexpr const T& value() const& {
			_throwOnNoValue();
			return private_optional.value().get();
		}

		[[nodiscard]]
		constexpr const T&& value() const&& {
			_throwOnNoValue();
			return std::move(private_optional.value().get());
		}

		[[nodiscard]]
		constexpr T& value() & {
			_throwOnNoValue();
			return private_optional.value().get();
		}

		[[nodiscard]]
		constexpr T&& value() && {
			_throwOnNoValue();
			return std::move(private_optional.value().get());
		}

		[[nodiscard]]
		const T& value_or(const T& or_value) const& {
			if (has_value()) return value();
			return or_value;
		}

		[[nodiscard]]
		const T&& value_or(const T&& or_value) const&& {
			if (has_value()) return std::move(value());
			return std::move(or_value);
		}

		[[nodiscard]]
		T& value_or(T& or_value) & {
			if (has_value()) return value();
			return or_value;
		}

		[[nodiscard]]
		T&& value_or(T&& or_value) && {
			if (has_value()) return std::move(value());
			return std::move(or_value);
		}

		// clang-format off
		// Turning clang-format, because it cannot format the following functions correctly.
		[[nodiscard]]
		constexpr const T& operator*() const& {
			return value();
		}

		[[nodiscard]]
		constexpr const T&& operator*() const&& {
			return std::move(value());
		}

		[[nodiscard]]
		constexpr T& operator*() & {
			return value();
		}

		[[nodiscard]]
		constexpr T&& operator*() && {
			return std::move(value());
		}

		[[nodiscard]]
		constexpr const T*
		operator->() const {
			_throwOnNoValue();
			return &value();
		}

		[[nodiscard]]
		constexpr T*
		operator->() {
			_throwOnNoValue();
			return &value();
		}

		// clang-format on

		[[nodiscard]]
		constexpr const T& expect(std::string_view message) const& {
			if (!has_value()) CORE_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr const T&& expect(std::string_view message) const&& {
			if (!has_value()) CORE_PANIC(message);
			return std::move(value());
		}

		[[nodiscard]]
		constexpr T& expect(std::string_view message) & {
			if (!has_value()) CORE_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr T&& expect(std::string_view message) && {
			if (!has_value()) CORE_PANIC(message);
			return std::move(value());
		}

		template<typename Function>
		auto map(const Function& function) const -> Optional<std::invoke_result_t<Function, T>> {
			if (has_value()) return function(value());
			return {};
		}

		template<typename Function>
		auto map(const Function& function) -> Optional<std::invoke_result_t<Function, T>> {
			if (has_value()) return function(value());
			return {};
		}

		template<typename Function>
		auto flatMap(const Function& function) const -> std::invoke_result_t<Function, T> {
			static_assert(IsOfSameClass<std::invoke_result_t<Function, T>, Optional>);
			if (has_value()) return function(value());
			return {};
		}

		template<typename Function>
		auto flatMap(const Function& function) -> std::invoke_result_t<Function, T> {
			static_assert(IsOfSameClass<std::invoke_result_t<Function, T>, Optional>);
			if (has_value()) return function(value());
			return {};
		}

	private:
		void _throwOnNoValue() const {
			if (!has_value()) CORE_PANIC("Tried to retrieve a value from an empty optional.");
		}

		Optional<std::reference_wrapper<T>> private_optional;
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
