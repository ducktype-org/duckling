/**
 * @file ref_optional.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "optional.hpp"

namespace base {
	template<class T>
	class OptionalReference {
	public:
		OptionalReference() = default;

		explicit OptionalReference(T& value): private_optional(std::ref(value)) {}

		explicit OptionalReference(const T& value): private_optional(std::cref(value)) {}

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

		template<typename Function>
		auto map(const Function& function) const {
			return private_optional.map(function);
		}

		// A non-const version.
		template<typename Function>
		auto map(const Function& function) {
			return private_optional.map(function);
		}

		template<typename Function>
		auto flatMap(const Function& function) const {
			return private_optional.flatMap(function);
		}

		// A non-const version.
		template<typename Function>
		auto flatMap(const Function& function) {
			return private_optional.flatMap(function);
		}


	private:
		void _throwOnNoValue() const {
			if (!has_value()) RIFT_PANIC("Tried to retrieve a value from an empty optional.");
		}

		Optional<std::reference_wrapper<T>> private_optional;
	};
}  // base
