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
	template<std::derived_from<LangElement> El, std::derived_from<LangElement> T, base::TemplateStringLiteral name>
	void clone(El& parent, AccessInternal<T, name>& sink, const AccessInternal<T, name>& source) {
		if (auto src = source.internal()) {
			MBox<T> copy = source.clone();
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
				MBox<T> copy = source.clone();
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
				MBox<T> copy = source.clone();
				copy->setParent(parent);
				*sink = std::move(copy);
			}
		}
	}

	template<std::derived_from<LangElement> El, std::derived_from<LangElement> T, base::TemplateStringLiteral name>
	void clone(El& parent, std::vector<AccessInternal<T, name>>& sink, const std::vector<AccessInternal<T, name>>& source) {
		if (source) {
			if (auto src = source->internal()) {
				MBox<T> copy = source.clone();
				copy->setParent(parent);
				std::string str_name(name.value);
				parent->addNamedChild(str_name, copy.refMut());
				*sink = std::move(copy);
			}
		}
	}
}

#define ELEMENT_CLONE_SUB_ELEMENT(sub_element_name) \
	clone(*out, out->sub_element_name, this->sub_element_name);

#define ELEMENT_CLONE_DEF(element_type, ...) \
	MBox<element_type> element_type::cloneElement() const {\
		Box<element_type> out = base::makeBox<element_type>(makeCloneDummy(), *this);\
		FOR_EACH(ELEMENT_CLONE_SUB_ELEMENT, __VA_ARGS__)\
		return {std::move(out)};\
	}