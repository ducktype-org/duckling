#include "../../../hierarchy/not_statements/wrapper_elements/operator_wrapper.hpp"

#include "../preamble.hpp"

namespace pst {

	MBox<OperatorWrapper> OperatorWrapper::parse(LangParserState& state) {
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
