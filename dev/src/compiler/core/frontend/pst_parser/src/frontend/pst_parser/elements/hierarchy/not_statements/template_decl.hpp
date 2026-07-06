#pragma once

#include "../lists/parameter_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Template declaration - the template:{} prefix of the template statement.
	 */
	class TemplateDecl final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateDecl, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		// @TODO: #3042 Probably change this from `()` syntax to `{}` or `[]` syntax
		NAMED_CHILD(params, ParamList);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		explicit TemplateDecl(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::TemplateDecl;
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		bool trailingSemicolon() override { return false; }

		static MBox<TemplateDecl> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;
		~TemplateDecl() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Declaration";
		}
	};
}
