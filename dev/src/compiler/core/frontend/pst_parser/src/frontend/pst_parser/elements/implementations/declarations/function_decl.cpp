#include "../../hierarchy/declarations/function_decl.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	// @TODO: make better
	MBox<FunDecl> FunDecl::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<FunDecl>(position);

		if (!assertStmtChoice<FunDecl>(state, state[0].is(Keyword::FunDecl))) return nullptr;

		state.parse(out).all(Keyword::FunDecl, &out->name);
		state.parse(out).one(&out->params);

		if (state.parse(out).tryEat(NamedOperator::SingleArrow)) state.parse(out).one(&out->ret);

		PST_RETURN out;
	}

	void FunDecl::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameters\":";
		nullAwareDprint(params, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << "}";
	}

	base::Optional<AccessLocked<ExprHolder>> FunDecl::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
	}

	LangElement::HashAlg& FunDecl::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void FunDecl::acceptVisitor(PstVisitor& visitor) const { visitor.visitFunDecl(*this); }
}
