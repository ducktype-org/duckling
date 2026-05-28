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
