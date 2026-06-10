#pragma once

#include "../lists/parameter_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration
	 */
	class FunDecl final: public Decl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FunDecl, Decl);
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD_OPT(ret, CommaExprHolder);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(FunDecl, ElementKind::FunDecl);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		bool trailingSemicolon() override { return true; }

		/**
		 * @note Optional of MCRef here is intentional
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getRet() const;

		static MBox<FunDecl> parse(LangParserState& state);
		void                 dprint(std::ostream& out) const final;
		~FunDecl() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function";
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
