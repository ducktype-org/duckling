#pragma once

namespace base {

	/**
	 * Type that can be constructed from any type and any type can be assigned to it, but nothing
	 * happens. Used to ignore values intentionally.
	 */
	struct Ignore final {
		Ignore() = default;

		template<typename T>
		Ignore(const T&) {}

		template<typename T>
		Ignore& operator=(const T&) {
			return *this;
		}
	};

}
