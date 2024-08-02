#pragma once

#include "../../rift_parser_state.hpp"
#include "../../pst_visitor.hpp" // IWYU pragma: export

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/parser_ref.hpp>

#include <rift_definitions/key_spec_op.hpp>
#include <rift_definitions/operator_precedence.hpp>
#include <lexer/token.hpp>
#include <lexer/classifications.hpp>

#include <base/variant.hpp>

#include <base/exceptions.hpp>
#include <ostream>
#include <functional> // IWYU pragma: export

namespace pst {
	using tpc::makeRef;

	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	using lexer::Token;

	/**
	 * @note This error that should generally not happen outside of our errors.
	 */
	template<typename Type>
	class BadStatementChoice final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			std::stringstream ss;
			ss << "Token doesn't match with chosen statement of `";
			ss << base::typeName<Type>() << "`.";
			return ss.str();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadStatementChoice(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<typename Type>
	bool assertStmtChoice(RiftParserState& state, bool good) {
		if (!good) {
			state.log(
				base::make_unique<BadStatementChoice<Type>>(state.ctokens().peek().getPosition())
			);
		}
		return good;
	}
}