// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/wrapper_elements/identifier_wrapper.hpp"

#include "../preamble.hpp"

namespace pst {

	MBox<IdentifierWrapper> IdentifierWrapper::parse(LangParserState& state) {
		auto name = state[0].getValue();
		bool good = state[0].isIdentifier();

		if (not good) {
			state.logInt(base::makeBox<tpc::NoIdentifierError>(
				state.getPosition(), state.ctokens().peek().describe()
			));
		}

		auto op = good ? name : base::StrID("bad identifier");

		Box<IdentifierWrapper> out = makeBox<IdentifierWrapper>(state, op);

		PARSE().eatOne();

		PST_RETURN out;
	}

	/**
	 * @brief Parses a function name: an identifier or a non-reserved operator symbol.
	 */
	MBox<IdentifierWrapper> IdentifierWrapper::parseFunctionName(LangParserState& state) {
		if (!state[0].isOperatorSymbol()) return IdentifierWrapper::parse(state);

		auto op = state[0].asBinaryOperator().value();

		if (!op.isNotReserved()) {
			// Plain `=` has no base operator to suggest, so it counts as reserved.
			if (op.isAssignment() && op.asNamed() != lang_def::NamedOperator::Assign)
				state.logInt(makeBox<AssignmentOperatorFunNameError>(state.getPosition(), op.str()));
			else
				state.logInt(makeBox<ReservedOperatorFunNameError>(state.getPosition(), op.str()));
			return nullptr;
		}

		Box<IdentifierWrapper> out = makeBox<IdentifierWrapper>(state, state[0].getValue());

		PARSE().eatOne();

		PST_RETURN out;
	}

	HashAlg& IdentifierWrapper::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		return partial_hash;
	}

	void IdentifierWrapper::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitIdentifierWrapper(*this);
	}

	void IdentifierWrapper::dprint(std::ostream& out) const {
		out << "{";
		out << "\"value\" : ";
		tpc::nullAwareDprint(name, out);
		out << "}";
	}
}
