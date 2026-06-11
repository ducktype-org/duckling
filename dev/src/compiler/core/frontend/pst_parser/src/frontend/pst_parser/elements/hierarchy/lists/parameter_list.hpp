#pragma once

#include "../not_statements/param.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration parameter list.
	 */
	class ParamList final: public List<Param, internal::NameGetters::parameterList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ParamList, List);
		CLONE_SUBELEMENTS();
	public:
		explicit ParamList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::ParamList;
		}

		static MBox<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};
}
