// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/wrapper_elements/operator_wrapper.hpp"

#include "../preamble.hpp"

namespace pst {

	MBox<OperatorWrapper> OperatorWrapper::parse(LangParserState& state) {
		// A writable view is one operator with two keywords, like `ref mut`.
		if (state[0].isKeyword() && state[1].is(Keyword::Mut)) {
			const auto writable_view = lang_def::writableViewOperator(state[0].asKeyword());
			if (writable_view.has_value()) {
				Box<OperatorWrapper> out = makeBox<OperatorWrapper>(state, writable_view.value());
				state.tokens().skip();
				state.tokens().skip();
				PST_RETURN out;
			}
		}

		// For now we use binary operator as it's the least restrictive.
		auto opt = state[0].asBinaryOperator();

		if (not opt.has_value()) {
			state.logInt(base::makeBox<tpc::NoOperatorError>(
				state.getPosition(), state.ctokens().peek().describe()
			));
		}

		auto op = opt.has_value() ? opt.value() : NamedOperator::NotAnOperator;

		Box<OperatorWrapper> out = makeBox<OperatorWrapper>(state, op);

		PARSE().one(op);

		PST_RETURN out;
	}

	HashAlg& OperatorWrapper::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, op);
		return partial_hash;
	}

	void OperatorWrapper::dprint(std::ostream& out) const {
		out << "{";
		out << "\"value\" : ";
		tpc::nullAwareDprint(op, out);
		out << "}";
	}
}
