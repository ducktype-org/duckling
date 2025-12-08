#pragma once

#include "preamble.hpp"

namespace pst {

	/**
	 * @brief Every variable needs to have either a type or value.
	 */
	class VariableNoTypeAndValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected either a type or value.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		VariableNoTypeAndValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	/**
	 * @brief Common parsing method for const/var/let variable declarations.
	 */
	template<typename T, lang_def::Keyword key>
	MBox<T> parseVariableTemplate(pst::LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<T>(position);

		if (!assertStmtChoice<T>(state, state[0].is(key))) return nullptr;

		state.parse(out).all(key, &out->name);

		bool has_type = false, has_value = false;

		if (state[0].is(lang_def::NamedOperator::Colon)) {
			state.parse(out).eatOne();
			state.parse(out).one(&out->type);

			has_type = true;
		}

		if (state[0].is(lang_def::NamedOperator::Assign)) {
			state.parse(out).eatOne();
			state.parse(out).one(&out->value);

			has_value = true;
		}

		if (!has_value && !has_type)
			state.log(makeBox<VariableNoTypeAndValueError>(state.getPosition()));

		if constexpr (key == Keyword::Var)
			out->is_const = false;
		else if constexpr (key == Keyword::Let)
			out->is_const = true;

		return out;
	}

}
