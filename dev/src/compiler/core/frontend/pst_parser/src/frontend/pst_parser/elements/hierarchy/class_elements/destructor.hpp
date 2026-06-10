#pragma once

#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class destructor element.
	 */
	class Destructor final: public ClassSpecial {
		THIS_CLASS(Destructor);
		PARENT_CLASS(ClassSpecial);
		CLONE_SUBELEMENTS();
		CLONE_SIGNATURE_DEFAULT_OVERRIDE();
	protected:
		NAMED_CHILD(body, CodeBlock);

	public:
		ELEMENT_CLONE_DECL(Destructor);
		CLASS_STMT_SPEC_CONSTRUCTOR(Destructor);
		CLASS_STMT_PARSE(Destructor);

		~Destructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Destructor";
		}

		[[nodiscard]]
		base::Optional<base::StrID> getInternalSymbolName() const final {
			return base::StrID("destroy");
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
