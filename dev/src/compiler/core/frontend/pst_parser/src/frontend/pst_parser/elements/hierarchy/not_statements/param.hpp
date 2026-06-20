#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Declaration of a single function or pattern argument.
	 */
	class Param final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Param, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(type, UniversalExprHolder);
		NAMED_CHILD_OPT(initial, UniversalExprHolder);

	public:
		explicit Param(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::Param;
		}

		static MBox<Param> parse(LangParserState& state);
		~Param() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Parameter";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getType() const {
			return type.give();
		}

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<UniversalExprHolder>> getValue() const;

		void acceptVisitor(PstVisitor& visitor) const final;
	};
}
