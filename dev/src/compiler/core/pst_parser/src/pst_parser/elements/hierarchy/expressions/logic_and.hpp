#pragma once

#include "binary_operator.hpp"
#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `and` operator.
	 */
	class LogicAnd final: public BinaryOperator {
		using Lower = LogicNot;
		using Self  = LogicAnd;

	public:
		explicit LogicAnd(const dia::SourcePosition& position):
			  BinaryOperator(position, lang_def::keywordToStr(lang_def::Keyword::And), 730) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~LogicAnd() override = default;
	};
}
