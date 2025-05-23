#pragma once

#include "../not_statements.hpp"
#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class constructor element.
	 */
	class CopyConstructor final: public ClassSpecial {
		AccessInternal<ParamList> params;
		AccessInternal<InitList>  inits;
		AccessInternal<CodeBlock> body;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(CopyConstructor);
		CLASS_STMT_PARSE(CopyConstructor);

		~CopyConstructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Copy Constructor";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
