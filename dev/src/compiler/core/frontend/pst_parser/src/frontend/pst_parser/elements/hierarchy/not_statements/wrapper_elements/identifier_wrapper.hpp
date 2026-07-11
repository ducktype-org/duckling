#pragma once

#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Wrapper for an identifier, currently in some situations may hold an operator.
	 *
	 * @TODO: #3119 This should get streamlined here.
	 */
	class IdentifierWrapper final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(IdentifierWrapper, NotStmt, name);

	protected:
		base::StrID name;
	public:
		explicit IdentifierWrapper(LangParserState& state, base::StrID name):
			  NotStmt(state),
			  name(name) {
			element_kind = ElementKind::IdentifierWrapper;
		}
		static MBox<IdentifierWrapper> parse(LangParserState& state);
		/**
		 * @brief Separate parsing function that handles names that can also be operators (like in function definition).
	 	 *
	 	 * @TODO: #3119 This will probably end up being a separate subclass here.
		 */
		static MBox<IdentifierWrapper> parseFunctionName(LangParserState& state);
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
