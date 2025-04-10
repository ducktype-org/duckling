#pragma once

#include "lang_parser_element.hpp"

#include <pst_parser/access.hpp>

#include <base/ref.hpp>

#include <concepts>

namespace pst {
	/**
	 * This can be used as a key of a query taking just single PST element.
	 * @todo this is a perfect template for explicit instantiations, to speed up compilation
	 */
	template<std::derived_from<LangElement> T = LangElement>
	struct GenericPSTQueryKey {
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
		base::HashT customPerfectHash() const {
			return element.illegalAccess().value()->getID().asInt();
		}

		bool operator==(const GenericPSTQueryKey& other) const {
			if (element.illegalAccess().has_value() == other.element.illegalAccess().has_value())
				return true;
			if (!element.illegalAccess().has_value() || !other.element.illegalAccess().has_value())
				return false;
			return &*element.illegalAccess().value() == &*other.element.illegalAccess().value();
		}
	};
}
