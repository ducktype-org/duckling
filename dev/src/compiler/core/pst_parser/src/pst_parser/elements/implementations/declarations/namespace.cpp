#include "../../hierarchy/declarations/namespace.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Namespace> Namespace::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Namespace>(position);

		if (!assertStmtChoice<Namespace>(state, state[0].is(Keyword::Namespace))) return nullptr;

		state.parse(out).all(Keyword::Namespace, &out->name).withDef(&out->body, CodeBlock::CodeBlockType::Unordered);

		return out;
	}

	void Namespace::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);

		// @TODO: change to body in print:
		out << R"(, "block": )";
		nullAwareDprint(body, out);

		out << "}";
	}

	void Namespace::acceptVisitor(PstVisitor& visitor) const { visitor.visitNamespace(*this); }
}
