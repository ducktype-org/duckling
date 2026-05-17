#include "../../hierarchy/declarations/function_decl.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	// @TODO: make better
	MBox<FunDecl> FunDecl::parse(LangParserState& state) {
		auto out = makeBox<FunDecl>(state);

		if (!assertStmtChoice<FunDecl>(state, state[0].is(Keyword::FunDecl))) return nullptr;

		PARSE().all(Keyword::FunDecl, &out->name);
		PARSE().one(&out->params);

		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

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

	HashAlg& FunDecl::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void FunDecl::acceptVisitor(PstVisitor& visitor) const { visitor.visitFunDecl(*this); }
}
