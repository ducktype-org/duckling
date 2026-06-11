#pragma once

#include "../not_statements/param.hpp"
#include "preamble.hpp"

namespace pst {
	namespace {
		using ParamParentList = List<Param, internal::NameGetters::parameterList>;
	}

	/**
	 * @brief Function declaration parameter list.
	 */
	class ParamList final: public ParamParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ParamList, ParamParentList);
	public:
		explicit ParamList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::ParamList;
		}

		static MBox<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};
}
