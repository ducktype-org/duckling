#pragma once

#include "preamble.hpp"

namespace pst {
	namespace {
		using AtrArgParentList = List<UniversalExprHolder, internal::NameGetters::attributeArgList>;
	}

	/**
	 * @brief Attribute argument list.
	 */
	class AtrArgList final: public AtrArgParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(AtrArgList, AtrArgParentList);
		CLONE_SUBELEMENTS();
	public:
		explicit AtrArgList(const LangParserState& state): List(state) {}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};
}
