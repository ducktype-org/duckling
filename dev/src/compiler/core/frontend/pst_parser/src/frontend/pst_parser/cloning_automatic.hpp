/**
 * @file cloning_automatic.hpp
 * 
 * Extensions of pst_automatic used for element cloning.
 * Utilities for cloning that are supposed to be used in definitions, shouldn't be included outside of element implementations.
 *
 * @todo reduce the repeating code by adding functions to pst_automatic
 */
#pragma once

#include "pst_automatic.hpp"
#include "cloning_decl.hpp"

namespace pst::cloning_utils {
	template<std::derived_from<LangElement> El>
	MBox<El> cloneCast(Ref<El> original) {
		MBox<LangElement> lang_element = original.cloneElement();
		return dynamic_cast<MBox<El>>(lang_element);
	}

	template<std::derived_from<LangElement> El, std::derived_from<LangElement> T, base::TemplateStringLiteral name>
	void clone(El& parent, AccessInternal<T, name>& sink, const AccessInternal<T, name>& source) {
		if (auto src = source.internal()) {
			MBox<T> copy = cloneCast(src.toOpt().value());
			copy->setParent(parent);
			std::string str_name(name.value);
			parent->addNamedChild(str_name, copy.refMut());
			*sink = std::move(copy);
		}
	}

	template<std::derived_from<LangElement> El, std::derived_from<LangElement> T, base::TemplateStringLiteral name>
	void clone(El& parent, base::Optional<AccessInternal<T, name>>& sink, const base::Optional<AccessInternal<T, name>>& source) {
		if (source) {
			if (auto src = source->internal()) {
				MBox<T> copy = cloneCast(src.toOpt().value());
				copy->setParent(parent);
				std::string str_name(name.value);
				parent->addNamedChild(str_name, copy.refMut());
				*sink = std::move(copy);
			}
		}
	}

	template<std::derived_from<LangElement> El, std::derived_from<LangElement> T>
	void clone(El& parent, base::Optional<AccessInternalAnonymous<T>>& sink, const base::Optional<AccessInternalAnonymous<T>>& source) {
		if (source) {
			if (auto src = source->internal()) {
				MBox<T> copy = cloneCast(src.toOpt().value());
				copy->setParent(parent);
				*sink = std::move(copy);
			}
		}
	}

	template<std::derived_from<LangElement> El, std::derived_from<LangElement> T>
	void clone(El& parent, std::vector<AccessInternalAnonymous<T>>& sink, const std::vector<AccessInternalAnonymous<T>>& source) {
		if (source) {
			for (auto& ref: source) {
				sink.push_back(nullptr);
				clone(parent, sink.back(), ref);
			}
		}
	}
}

#define ELEMENT_CLONE_SUB_ELEMENT(sub_element_name) \
	pst::cloning_utils::clone(*this, sub_element_name, other.sub_element_name);

#define CLONE_SUB_ELEMENTS_DEF(element_type, ...) \
	void element_type::cloneSubElements(const element_type& other) {\
		FOR_EACH(ELEMENT_CLONE_SUB_ELEMENT, __VA_ARGS__)\
		ParentClass::cloneSubElements(other);\
	}