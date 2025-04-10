#pragma once

#include "../lang_parser_element.hpp"

#include <vector>

namespace pst {

	/**
	 * @brief Generates vector that contain Refs to all elements in the subtree of the root element
	 * (recursively). Order of elements is arbitrary. Should probably be used for tests only.
	 */
	std::vector<AccessLocked<pst::LangElement>>
		viewAllSubTreeElements(AccessLocked<pst::LangElement> root);

	/**
	 * @brief Same as viewAllSubTreeElements,
	 * but filters out elements that are subtype of type T.
	 * Should probably be used for tests only. Might be slow, due to dynamic_cast's.
	 */
	template<class T>
	std::vector<AccessLocked<T>> viewAllSubTreeElementsFillter(AccessLocked<pst::LangElement> root
	) {
		auto                         all = viewAllSubTreeElements(root);
		std::vector<AccessLocked<T>> result;
		for (auto el: all) {
			auto casted = el.dynamicCast<T>().illegalAccess();
			if (casted) result.push_back(casted.value());
		}
		return result;
	}
}
