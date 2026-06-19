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
		explicit AtrArgList(const LangParserState& state): List(state) {}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};
}
