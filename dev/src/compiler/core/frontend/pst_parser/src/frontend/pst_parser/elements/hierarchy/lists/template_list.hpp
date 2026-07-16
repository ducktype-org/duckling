#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Template initialization list.
	 */
	class TemplateList final:
		  public List<UniversalExprHolderLowerLevel, internal::NameGetters::templateList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit TemplateList(LangElementConstructionArgument state): List(state) {
			this->element_kind = ElementKind::TemplateList;
		}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
