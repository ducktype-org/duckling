#pragma once

#include "../../meta.hpp"

namespace pst {
	class IdentifierWrapper final: public NotStmt {
		base::StrID name;

	public:
		explicit IdentifierWrapper(LangParserState& state, base::StrID name):
			  NotStmt(state),
			  name(name) {
			element_kind = ElementKind::IdentifierWrapper;
		}

		static MBox<IdentifierWrapper> parse(LangParserState& state);
		~IdentifierWrapper() final = default;

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		base::StrID unwrap() const {
			return name;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Identifier Wrapper";
		}

		// @TODO: #2782 Remove this.
		void acceptVisitor(PstVisitor& visitor) const final;
	};
}
