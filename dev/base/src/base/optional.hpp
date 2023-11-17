/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <optional>
#include "exceptions.hpp"
#include "type_traits.hpp"
#include "unique_pointer.hpp"
#include <result.hpp>
#include <functional>


/* Some c00l macros.
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
 * if_opt_some(test, value) {
 * 	std::cout << "Test has value of: " << value << "\n";
 * }
 *
 * if_opt_none(test) {
 * 	std::cout << "Test has no value.\n";
 * }
 *
 */
#define match_optional(optional)                                                                \
	PUSH_DIAGNOSTIC                                                                             \
	NO_SHADOW                                                                                   \
	if (bool _perform_match = true)                                                             \
		for (const auto& _internal_optional = optional; _perform_match; _perform_match = false) \
	POP_DIAGNOSTIC

#define opt_some(_value_name)                                                   \
	PUSH_DIAGNOSTIC                                                             \
	NO_SHADOW                                                                   \
	if (bool _perform_if = _internal_optional.has_value())                      \
		for (const auto& _value_name = _internal_optional.value(); _perform_if; \
		     _perform_if             = false)                                   \
	POP_DIAGNOSTIC

#define opt_none    \
	PUSH_DIAGNOSTIC \
	NO_SHADOW       \
	if (!_internal_optional.has_value()) POP_DIAGNOSTIC

#define if_opt_some(optional, _value_name)                                                 \
	PUSH_DIAGNOSTIC                                                                        \
	NO_SHADOW                                                                              \
	if (bool _perform_if = optional.has_value())                                           \
		for (const auto& _value_name = optional.value(); _perform_if; _perform_if = false) \
	POP_DIAGNOSTIC

#define if_opt_none(optional) \
	PUSH_DIAGNOSTIC           \
	NO_SHADOW                 \
	if (!optional.has_value()) POP_DIAGNOSTIC

namespace base {
	/**
	 * Optional is used as a better and safer alternative to pointers. It's name naturally suggests,
	 * that it may or may not hold a value underneath, and the programmer is responsible to first
	 * check for it's presence.
	 *
	 * Macros defined above may come in handy when dealing with these creatures.
	 * Also methods - map and flatMap - are very useful.
	 *
	 * Optional does not inherit from std::optional, because std::optional doesn't throw on
	 * null-value access, but base::optional does.
	 *
	 * @tparam T
	 */
	template<class T>
	class Optional {
	public:
		Optional() = default;

		Optional(const T& value): private_optional(value) {}

		template<class... Args>
		explicit Optional(Args&&... args):
			  private_optional(std::make_optional<T>(std::forward<Args>(args)...)) {}

		[[nodiscard]]
		virtual constexpr bool has_value() const {
			return private_optional.has_value();
		}

		[[nodiscard]]
		constexpr bool empty() const {
			return !has_value();
		}

		// Accessors.
		[[nodiscard]]
		virtual constexpr const T& value() const& {
			_throwOnNoValue();
			return private_optional.value();
		}

		[[nodiscard]]
		virtual constexpr const T&& value() const&& {
			_throwOnNoValue();
			return std::move(private_optional.value());
		}

		[[nodiscard]]
		virtual constexpr T& value() & {
			_throwOnNoValue();
			return private_optional.value();
		}

		[[nodiscard]]
		virtual constexpr T&& value() && {
			_throwOnNoValue();
			return std::move(private_optional.value());
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

		// clang-format on

		[[nodiscard]]
		constexpr const T& expect(std::string_view message) const& {
			if (!has_value()) RIFT_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr const T&& expect(std::string_view message) const&& {
			if (!has_value()) RIFT_PANIC(message);
			return std::move(value());
		}

		[[nodiscard]]
		constexpr T& expect(std::string_view message) & {
			if (!has_value()) RIFT_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr T&& expect(std::string_view message) && {
			if (!has_value()) RIFT_PANIC(message);
			return std::move(value());
		}

		/**
		 * Applies the passed function on the value and wraps in Optional if the object contains a
		 * value, otherwise does nothing.
		 * @tparam Function
		 * @param function Function to apply on the value. Function must take one argument which
		 * type has to match the optional's type (auto works too). Function can return any type of
		 * data.
		 * @return On value: Optional(function(value)), otherwise does
		 * nothing.
		 */
		template<typename Function>
		auto map(const Function& function) const -> Optional<decltype(function(T()))> {
			if (has_value()) return Optional<decltype(function(T()))>(function(value()));
			return {};
		}

		// A non-const version.
		template<typename Function>
		auto map(const Function& function) -> Optional<decltype(function(T()))> {
			if (has_value()) return Optional<decltype(function(T()))>(function(value()));
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
		auto flatMap(const Function& function) const -> decltype(function(T())) {
			static_assert(IsOfSameClass<decltype(function(T())), Optional>);
			if (has_value()) return decltype(function(T()))(function(value()));
			return {};
		}

		// A non-const version.
		template<typename Function>
		auto flatMap(const Function& function) -> decltype(function(T())) {
			static_assert(IsOfSameClass<decltype(function(T())), Optional>);
			if (has_value()) return decltype(function(T()))(function(value()));
			return {};
		}

	protected:
		void _throwOnNoValue() const {
			if (!has_value()) RIFT_PANIC("Tried to retrieve a value from an empty optional.");
		}

	private:
		std::optional<T> private_optional;
	};

	// @WARNING: This is almost an exact copy of the code above and there is pretty much nothing
	// we can do to avoid doing it this way, mainly because std::optional<T> is not supported for
	// T being an incomplete type.
	template<class T>
	class Optional<T&> {
	public:
		Optional() = default;

		explicit Optional(T& value): private_optional(std::ref(value)) {}

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

		// clang-format on

		[[nodiscard]]
		constexpr const T& expect(std::string_view message) const& {
			if (!has_value()) RIFT_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr const T&& expect(std::string_view message) const&& {
			if (!has_value()) RIFT_PANIC(message);
			return std::move(value());
		}

		[[nodiscard]]
		constexpr T& expect(std::string_view message) & {
			if (!has_value()) RIFT_PANIC(message);
			return value();
		}

		[[nodiscard]]
		constexpr T&& expect(std::string_view message) && {
			if (!has_value()) RIFT_PANIC(message);
			return std::move(value());
		}

		template<typename Function>
		auto map(const Function& function) const -> Optional<decltype(function(T()))> {
			if (has_value()) return Optional<decltype(function(T()))>(function(value()));
			return {};
		}

		// A non-const version.
		template<typename Function>
		auto map(const Function& function) -> Optional<decltype(function(T()))> {
			if (has_value()) return Optional<decltype(function(T()))>(function(value()));
			return {};
		}

		template<typename Function>
		auto flatMap(const Function& function) const -> decltype(function(T())) {
			static_assert(IsOfSameClass<decltype(function(T())), Optional>);
			if (has_value()) return decltype(function(T()))(function(value()));
			return {};
		}

		// A non-const version.
		template<typename Function>
		auto flatMap(const Function& function) -> decltype(function(T())) {
			static_assert(IsOfSameClass<decltype(function(T())), Optional>);
			if (has_value()) return decltype(function(T()))(function(value()));
			return {};
		}


	private:
		void _throwOnNoValue() const {
			if (!has_value()) RIFT_PANIC("Tried to retrieve a value from an empty optional.");
		}

		Optional<std::reference_wrapper<T>> private_optional;
	};
}  // base
