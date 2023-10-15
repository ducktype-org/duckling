/**
 * @file optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <optional>
#include "exceptions.hpp"

/* Some c00l macros.
 * Use like:
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
#define match_optional(optional)                                                                \
	PUSH_DIAGNOSTIC                                                                             \
	NO_SHADOW                                                                                   \
	if (bool _perform_match = true)                                                             \
		for (const auto& _internal_optional = optional; _perform_match; _perform_match = false) \
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

	template<class T>
	// Optional does not inherit from std::optional, because std::optional doesn't throw on
	// no-value access.
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

		[[nodiscard]]
		constexpr const T&
			operator*() const& {
			return value();
		}

		[[nodiscard]]
		constexpr const T&&
			operator*() const&& {
			return value();
		}

		[[nodiscard]]
		constexpr T&
			operator*() & {
			return value();
		}

		[[nodiscard]]
		constexpr T&&
			operator*() && {
			return value();
		}

		template<typename Fn>
		auto map(const Fn& func) -> Optional<decltype(func(T()))> {
			if (has_value()) return Optional<decltype(func(T()))>(func(value()));
			return {};
		}

		// Idea of a flatMap is simple: If "func" returns Optional<X>, then we use a flat map,
		// and we don't end up with Optional<Optional<X>>, but Optional<X>.
		template<typename Fn>
		auto flatMap(const Fn& func) -> decltype(func(T())) {
			static_assert(is_instance<decltype(func(T())), Optional>::value);
			if (has_value()) return decltype(func(T()))(func(value()));
			return {};
		}


	private:
		void _throwOnNoValue() const {
			if (!has_value()) RIFT_PANIC("Tried to retrieve a value from an empty optional.");
		}

		std::optional<T> private_optional;

		// Helpers for flatMap
		template<class, template<class> class>
		struct is_instance: public std::false_type {};

		template<class T2, template<class> class U>
		struct is_instance<U<T2>, U>: public std::true_type {};
	};


}  // base
