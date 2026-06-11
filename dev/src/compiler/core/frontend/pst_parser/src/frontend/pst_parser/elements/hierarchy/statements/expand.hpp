#pragma once

#include "../../../lang_parser_context.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Simple expand macro.
	 */
	class Expand final: public Stmt {
		THIS_CLASS(Expand);
		PARENT_CLASS(Stmt);
		CLONE_SIGNATURE_DEFAULT_OVERRIDE();
		CLONE_SUBELEMENTS();

	protected:
		// This is context that is saved during parsing so that it can be restored on expansion.
		Box<LangParserContext> context;
		NAMED_CHILD(value, CommaExprHolder);

	public:
		/**
		 */
		explicit Expand(pst::CloneDummy dummy, const Expand& other):
			  ParentClass(dummy, other),
			  context(makeBox<LangParserContext>(other.context.ref())) {}

		Expand(const LangParserState& state, CRef<LangParserContext> context):
			  Stmt(StmtKind::Expand, state),
			  context(makeBox<LangParserContext>(context)) {
			this->element_kind = ElementKind::Expand;
		}

		static MBox<Expand> parse(LangParserState& state);

		~Expand() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		AccessLocked<CommaExprHolder> getValue() const {
			return value.give();
		}

		[[nodiscard]]
		CRef<LangParserContext> getContext() const {
			return context.ref();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Expand";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}
	};
}
