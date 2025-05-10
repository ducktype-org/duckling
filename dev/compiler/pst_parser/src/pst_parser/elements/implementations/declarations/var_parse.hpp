#pragma once

#include "preamble.hpp"

namespace pst {

	template<typename T, lang_def::Keyword key>
	MBox<T> parseVariableTemplate(pst::LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<T>(position);

		if (!assertStmtChoice<T>(state, state[0].is(key))) return nullptr;

		// @TODO: Add a possibility for type deduction from assigned value and no initial value.
		state.parse(out).all(key, &out->name, lang_def::NamedOperator::Colon);

		state.parse(out).one(&out->type);

		state.parse(out).one(lang_def::NamedOperator::Assign, true);

		// @TODO: Perhaps add possibility for default construction.
		state.parse(out).one(&out->value);

		return out;
	}

}
