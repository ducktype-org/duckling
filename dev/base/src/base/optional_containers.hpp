/**
 * @file optional_containers.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <optional>

namespace base {
	template<class T>
	class OptionalContainer {
	public:
		// Accessors.
		[[nodiscard]]
		virtual constexpr const T& value() const& = 0;

		[[nodiscard]]
		virtual constexpr const T&& value() const&& = 0;

		[[nodiscard]]
		virtual constexpr T& value() & = 0;

		[[nodiscard]]
		virtual constexpr T&& value() && = 0;
	};

	namespace optional_containers {
		template<class T>
		class StdOptionalContainer: public OptionalContainer<T> {
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

		private:
			std::optional<T> private_optional;
		};

		template<class T>
		class UniquePtrContainer: public OptionalContainer<T> {
			...
		};
	}
}
