#pragma once

#include "elements/hierarchy/not_statements/synthetic_elements/template_top_level.hpp"
#include "pst.hpp"

namespace pst {
	/**
	 * @brief PST holder for generated sub trees. For now will be used for template expansion.
	 *
	 * For now this holds minimal information.
	 */
	template<std::derived_from<LangElement> Element = TemplateTopLevel>
	class TemplateExpansionPST final: public PST<Element> {
	private:
		TemplateExpansionPST(Box<Element>&& el, hashing::ComponentHash&& hash_ctx):
			  PST<Element>(std::move(hash_ctx)) {
			PST<Element>::assignRoot(std::move(el));

			PST<Element>::finishGeneratedPST();
		}

	public:
		/**
		 * @brief A custom makeBox with extended visibility.
		 */
		template<typename... Args>
		static auto makeTemplateExpansionPstBox(Args&&... args) {
			return Box<TemplateExpansionPST>::fromPointer(
				new TemplateExpansionPST(std::forward<Args>(args)...)
			);
		}

		static Box<TemplateExpansionPST> fromElement(
			Box<Element>&& el, hashing::ComponentHash&& hash_ctx = {}
		) {
			auto out = makeTemplateExpansionPstBox(std::move(el), std::move(hash_ctx));

			return out;
		}

		[[nodiscard]] bool hasErrors() const override { return false; }

		~TemplateExpansionPST() final = default;
	};
}
