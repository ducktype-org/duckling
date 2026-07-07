#pragma once

#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class destructor element.
	 */
	class Destructor final: public ClassSpecial {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Destructor, ClassSpecial);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(body, CodeBlock);

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Destructor);
		PARSE_DECL();

		~Destructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		AccessLocked<CodeBlock> getBody() const {
			return body.give();
		}

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
