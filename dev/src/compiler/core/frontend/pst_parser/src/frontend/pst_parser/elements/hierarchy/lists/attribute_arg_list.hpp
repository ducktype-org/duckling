#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Attribute argument list.
	 */
	class AtrArgList final:
		  public List<UniversalExprHolder, internal::NameGetters::attributeArgList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(AtrArgList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit AtrArgList(LangElementConstructionArgument state): List(state) {
			this->element_kind = ElementKind::AtrArgList;
		}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};
}
