/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <optional>
#include "exceptions.hpp"
#include "type_traits.hpp"

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
 */
#define match_optional(optional)                                                          \
	PUSH_DIAGNOSTIC                                                                       \
	NO_SHADOW                                                                             \
	if (bool _perform_match = true)                                                       \
		for (auto& _internal_optional = optional; _perform_match; _perform_match = false) \
	POP_DIAGNOSTIC

#define opt_some(_value_name)                                                                  \
	PUSH_DIAGNOSTIC                                                                            \
	NO_SHADOW                                                                                  \
	if (bool _perform_if = _internal_optional.has_value())                                     \
		for (auto& _value_name = _internal_optional.value(); _perform_if; _perform_if = false) \
	POP_DIAGNOSTIC

#define opt_none    \
	PUSH_DIAGNOSTIC \
	NO_SHADOW       \
	if (!_internal_optional.has_value()) POP_DIAGNOSTIC

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

		explicit Optional(T value): private_optional(std::make_optional<T>(value)) {}

		template<class... Args>
		explicit Optional(Args&&... args):
			  private_optional(std::make_optional<T>(std::forward<Args>(args)...)) {}

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
			return *private_optional;
		}

		[[nodiscard]]
		constexpr const T&& value() const&& {
			_throwOnNoValue();
			return std::move(private_optional);
		}

		[[nodiscard]]
		constexpr T& value() & {
			_throwOnNoValue();
			return *private_optional;
		}

		[[nodiscard]]
		constexpr T&& value() && {
			_throwOnNoValue();
			return std::move(private_optional);
		}

		// clang-format off
		// Turning clang-format, because it cannot format the following functions correctly.
		[[nodiscard]]
		constexpr const T& operator*() const& {
			return value();
		}

		[[nodiscard]]
		constexpr const T&& operator*() const&& {
			return value();
		}

		[[nodiscard]]
		constexpr T& operator*() & {
			return value();
		}

		[[nodiscard]]
		constexpr T&& operator*() && {
			return value();
		}

		// clang-format on


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
		auto flatMap(const Function& function) -> decltype(function(T())) {
			static_assert(IsOfSameClass<decltype(function(T())), Optional>);
			if (has_value()) return decltype(function(T()))(function(value()));
			return {};
		}


	private:
		void _throwOnNoValue() const {
			if (!has_value()) RIFT_PANIC("Tried to retrieve a value from an empty optional.");
		}

		std::optional<T> private_optional;
	};


}  // base
