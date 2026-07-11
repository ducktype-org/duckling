#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Special top-level element for instantiated templates.
	 */
	class TemplateExpansionAssignment final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateExpansionAssignment, NotStmt);	
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD_OPT(type, CommaExprHolder);
		NAMED_CHILD_OPT(value, CommaExprHolder);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		void                      dprint(std::ostream& out) const final;
		~TemplateExpansionAssignment() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Expansion Assignment";
		}
	};
}