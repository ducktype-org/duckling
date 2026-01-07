#include "../../hierarchy/class_elements/destructor.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst {
	class NonEmptyError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "destructor_arguments_error" };
		}

	public:
		NonEmptyError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<Destructor> Destructor::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<Destructor>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).eatOne();

		tpc::Identifier ident;
		state.parse(out).all(NamedOperator::Period, &ident);
		out->kind = ident;

		state.parse(out).goDown();
		if (state.notEmpty()) state.logInt(makeBox<NonEmptyError>(state.getPosition()));
		state.parse(out).goUpAndSkip();

		state.parse(out)
			.one(NamedOperator::Assign)
			.withDef(&out->body, CodeBlock::CodeBlockType::Ordered);

		return out;
	}

	void Destructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Destructor::acceptVisitor(PstVisitor& visitor) const { visitor.visitDestructor(*this); }
}
