#pragma once

#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class constructor element.
	 */
	class Constructor final: public ClassSpecial {
		AccessInternal<ParamList> params;
		AccessInternal<InitList>  inits;
		AccessInternal<CodeBlock> body;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Constructor);
		CLASS_STMT_PARSE(Constructor);

		~Constructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Constructor";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
