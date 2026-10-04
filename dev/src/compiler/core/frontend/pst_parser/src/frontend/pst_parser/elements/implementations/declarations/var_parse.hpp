#pragma once

#include "preamble.hpp"

#include <diagnostic/message.hpp>

namespace pst {

	/**
	 * @brief Every variable needs to have either a type or value.
	 */
	class VariableNoTypeAndValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "variable_no_type_and_value" };
		}

	public:
		VariableNoTypeAndValueError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	/**
	 * @brief Common parsing method for const/var/let variable declarations.
	 */
	template<typename T, lang_def::Keyword key>
	MBox<T> parseVariableTemplate(pst::LangParserState& state) {
		auto out = makeBox<T>(state);

		if (!assertStmtChoice<T>(state, state[0].is(key))) return nullptr;

		PARSE().all(key, &out->name);

		bool has_type = false, has_value = false;

		if (state[0].is(lang_def::NamedOperator::Colon)) {
			PARSE().eatOne();
			PARSE().one(&out->type);

			has_type = true;
		}

		if (state[0].is(lang_def::NamedOperator::Assign)) {
			PARSE().eatOne();
			PARSE().one(&out->value);

			has_value = true;
		}

		if (!has_value && !has_type)
			state.logInt(makeBox<VariableNoTypeAndValueError>(state.getPosition()));

		if constexpr (key == Keyword::Var)
			out->is_const = false;
		else if constexpr (key == Keyword::Let)
			out->is_const = true;

		PST_RETURN out;
	}

}
