#pragma once

#include "../elements.hpp"

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/parser_ref.hpp>

#include <rift_definitions/key_spec_op.hpp>
#include <rift_definitions/operator_precedence.hpp>
#include <lexer/token.hpp>
#include <lexer/classifications.hpp>

#include <base/exceptions.hpp>
#include <ostream>
#include <variant>
#include <functional>

namespace pst {
	using tpc::makeRef;
	using tpc::parseAll;
	using tpc::parseOne;

	using rift_def::Keyword;
	using rift_def::Operator;
	using rift_def::Special;

	using lexer::Token;
}
