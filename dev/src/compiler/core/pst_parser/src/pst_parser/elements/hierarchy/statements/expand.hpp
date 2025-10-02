#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Simple expand macro
	 */
	class Expand final: public Stmt {
		NAMED_CHILD(value, CommaExprHolder);

	public:
		STMT_CHILD_CONSTRUCTOR(Expand, ElementKind::Expand);
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
		std::string elementType() const override {
			return "Expand";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}
	};
}
