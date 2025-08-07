#pragma once

#include "../lists/initializer_list.hpp"
#include "../lists/parameter_list.hpp"
#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class constructor element.
	 */
	class Constructor final: public ClassSpecial {
		NAMED_CHILD(params, ParamList);	
		NAMED_CHILD(inits, InitList);	
		NAMED_CHILD(body, CodeBlock);	

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Constructor);
		CLASS_STMT_PARSE(Constructor);

		~Constructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Constructor";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
