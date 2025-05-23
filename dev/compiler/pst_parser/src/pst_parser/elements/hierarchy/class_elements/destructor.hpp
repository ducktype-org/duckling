#pragma once

#include "../not_statements.hpp"
#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class destructor element.
	 */
	class Destructor final: public ClassSpecial {
		AccessInternal<CodeBlock> body;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Destructor);
		CLASS_STMT_PARSE(Destructor);

		~Destructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Destructor";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
