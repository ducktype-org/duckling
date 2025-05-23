#pragma once

#include "../not_statements.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class method element.
	 */
	class Method final: public ClassStmt {
		tpc::Identifier                                 name;
		AccessInternal<ParamList>                       params;
		base::Optional<AccessInternal<CommaExprHolder>> ret;
		AccessInternal<CodeBlock>                       body;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Method, ElementKind::ClassMethod);
		CLASS_STMT_PARSE(Method);

		~Method() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Method";
		}

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return false;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
