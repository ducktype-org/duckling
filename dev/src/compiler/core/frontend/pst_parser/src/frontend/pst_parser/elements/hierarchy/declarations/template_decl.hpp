#pragma once

#include "../lists/parameter_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Template declaration
	 */
	class TemplateDecl final: public Decl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateDecl, Decl);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(params, ParamList);  // {} vs (), mock for now
		NAMED_CHILD(inner_statement, Stmt);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(TemplateDecl, ElementKind::Template);

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
            return params.give();
		}
        
        [[nodiscard]]
        AccessLocked<Stmt> getInnerStatement() const {
            return inner_statement.give();
        }

		bool trailingSemicolon() override { return false; } 


		static MBox<TemplateDecl> parse(LangParserState& state);
		void                 dprint(std::ostream& out) const final;
		~TemplateDecl() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Declaration";
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
            // TODO PR: this breaks incremental maybe?
			return inner_statement.internal()->getDeclSymbolIdentifier();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
