#pragma once

#include "elements/hierarchy/not_statements/synthetic_elements/template_top_level.hpp"
#include "pst.hpp"

namespace pst {
	/**
	 * @brief PST holder for generated sub trees. For now will be used for template expansion.
	 *
	 * @TODO: Maybe add some information about the original PST.
	 */
	template<std::derived_from<LangElement> Element = TemplateTopLevel>
	class TemplateExpansionPST final: public PST<Element> {
	private:
		TemplateExpansionPST(Box<Element>&& el, hashing::ComponentHash&& hash_ctx):
			  PST<Element>(std::move(hash_ctx)) {
			assignRoot(std::move(el));

			PST<Element>::finishGeneratedPST();
		}

	public:
		static Box<TemplateExpansionPST> fromElement(
			Box<Element>&& el, hashing::ComponentHash&& hash_ctx = {}
		) {
			auto out = makeBox<TemplateExpansionPST>(std::move(el), std::move(hash_ctx));

			return out;
		}

		[[nodiscard]] bool hasErrors() const override { return false; }
	};
}
