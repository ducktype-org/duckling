// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lang_parser_element.hpp"

#include <frontend/pst_parser/access.hpp>

#include <base/pointers/ref.hpp>

namespace pst {
	/**
	 * This can be used as a key of a query taking just single PST element.
	 * @todo this is a perfect template for explicit instantiations, to speed up compilation
	 */
	template<typename /*std::derived_from<LangElement>*/ T = LangElement>
	struct GenericPSTQueryKey final {
		template<typename E>
		GenericPSTQueryKey(const AccessLocked<E>& element) noexcept: element(element){};
		template<typename E>
		GenericPSTQueryKey(const Access<E>& element) noexcept: element(element){};
		template<typename E>
		GenericPSTQueryKey(const MCRef<E>& ref) noexcept: element(ref){};
		/**
		 * @brief Element for which the query is run.
		 * @TODO: this is an MCRef, since all keys ware defined like this
		 * after PST "boxification". We should decide how HELIOS handles PST nulls,
		 * and have a single convention.
		 */
		AccessLocked<T> element;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			// Note: this hash is also stable if someone needs it.
			if (element.illegalAccess().empty()) {
				// nullptr element, i.e. element that failed to parse.
				// We can return any constant here,
				// hash will still be collision resistant, and we don't provide any
				// bits of input by this key.
				return base::Bit256{ 0 };
			}
			return element.illegalAccess().value()->getHash();
		}
	};
}
