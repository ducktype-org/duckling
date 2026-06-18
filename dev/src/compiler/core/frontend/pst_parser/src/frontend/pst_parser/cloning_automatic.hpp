/**
 * @file cloning_automatic.hpp
 *
 * Extensions of pst_automatic used for element cloning.
 * Utilities for cloning that are supposed to be used in definitions, shouldn't be included outside
 * of element implementations.
 *
 * @TODO: #2937 reduce the repeating code by adding functions to pst_automatic
 */
#pragma once

#include "cloning_decl.hpp"
#include "pst_automatic.hpp"

namespace pst {
	class CloningUtils {
	public:
		template<std::derived_from<LangElement> El>
		static MBox<El> cloneCast(CRef<El> original) {
			MBox<LangElement> lang_element = original->clone();
			return std::move(lang_element).dynamicCast<El>();
		}

		template<
			std::derived_from<LangElement> El,
			std::derived_from<LangElement> T,
			base::TemplateStringLiteral    name>
		static void clone(
			El& parent, AccessInternal<T, name>& sink, const AccessInternal<T, name>& source
		) {
			if (auto src = source.internal()) {
				MBox<T> copy = std::move(cloneCast(src.toOpt().value()));
				copy->setParent({ &parent });
				std::string str_name(name.value);
				parent.addNamedChild(str_name, copy.refMut());
				sink = std::move(copy);
			}
		}

		template<
			std::derived_from<LangElement> El,
			std::derived_from<LangElement> T,
			base::TemplateStringLiteral    name>
		static void clone(
			El&                                            parent,
			base::Optional<AccessInternal<T, name>>&       sink,
			const base::Optional<AccessInternal<T, name>>& source
		) {
			if (source) {
				if (source) {
					sink.emplace(nullptr);
					clone(parent, sink.value(), source.value());
				}
			}
		}

		template<std::derived_from<LangElement> El, std::derived_from<LangElement> T>
		static void clone(
			El& parent, AccessInternalAnonymous<T>& sink, const AccessInternalAnonymous<T>& source
		) {
			if (auto src = source.internal()) {
				MBox<T> copy = std::move(cloneCast(src.toOpt().value()));
				copy->setParent({ &parent });
				parent.addChild(copy);
				sink = std::move(copy);
			}
		}

		template<std::derived_from<LangElement> El, std::derived_from<LangElement> T>
		static void clone(
			El&                                               parent,
			base::Optional<AccessInternalAnonymous<T>>&       sink,
			const base::Optional<AccessInternalAnonymous<T>>& source
		) {
			if (source) {
				sink.emplace(nullptr);
				clone(parent, sink.value(), source.value());
			}
		}

		template<std::derived_from<LangElement> El, std::derived_from<LangElement> T>
		static void clone(
			El&                                            parent,
			std::vector<AccessInternalAnonymous<T>>&       sink,
			const std::vector<AccessInternalAnonymous<T>>& source
		) {
			for (auto& ref: source) {
				sink.push_back(nullptr);
				clone(parent, sink.back(), ref);
			}
		}

		template<std::derived_from<LangElement> El, std::derived_from<LangElement> T>
		static void clone(
			El&                                                            parent,
			base::Optional<std::vector<AccessInternalAnonymous<T>>>&       sink,
			const base::Optional<std::vector<AccessInternalAnonymous<T>>>& source
		) {
			if (source) {
				sink.emplace();
				clone(parent, sink.value(), source.value());
			}
		}
	};
}

#define ELEMENT_CLONE_SUB_ELEMENT(sub_element_name) \
	pst::CloningUtils::clone(*this, sub_element_name, other.sub_element_name);

#define CLONE_SUB_ELEMENTS_DEF(element_type, ...)                    \
	void element_type::cloneSubElements(const element_type& other) { \
		FOR_EACH(ELEMENT_CLONE_SUB_ELEMENT, __VA_ARGS__)             \
		ParentClass::cloneSubElements(other);                        \
	}
