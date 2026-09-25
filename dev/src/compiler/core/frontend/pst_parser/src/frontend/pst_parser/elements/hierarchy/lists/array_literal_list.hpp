#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration parameter list.
	 */
	class ArrayLiteralList final: public List<UniversalExprHolder, internal::NameGetters::arrayLiteralList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ArrayLiteralList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit ArrayLiteralList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::ArrayLiteralList;
		}

		static MBox<ArrayLiteralList> parse(LangParserState& state);

		~ArrayLiteralList() final = default;
	};
}
