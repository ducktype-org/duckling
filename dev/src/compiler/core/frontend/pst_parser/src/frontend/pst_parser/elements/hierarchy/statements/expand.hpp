#pragma once

#include "../meta.hpp"
#include "../../../lang_parser_context.hpp"

namespace pst {
	/**
	 * @brief Simple expand macro
	 */
	class Expand final: public Stmt {
		Box<LangParserContext> context;
		NAMED_CHILD(value, CommaExprHolder);

	public:
		Expand(const dia::SourcePosition& position, CRef<LangParserContext> context): Stmt(StmtKind::Expand, position), context(makeBox<LangParserContext>(context)) {
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
