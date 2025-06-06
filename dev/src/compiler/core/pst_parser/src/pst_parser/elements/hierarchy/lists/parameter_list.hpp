#pragma once

#include "../not_statements/fun_param.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration parameter list.
	 */
	class ParamList final: public List<FunParam, detail::NameGetters::parameterList> {
	public:
		explicit ParamList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::ParamList;
		}

		static MBox<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};
}
