#pragma once

#include "../lists/initializer_list.hpp"
#include "../lists/parameter_list.hpp"
#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class copy constructor element.
	 */
	class CopyConstructor final: public ClassSpecial {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CopyConstructor, ClassSpecial);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD(inits, InitList);
		NAMED_CHILD(body, CodeBlockOrStmt);

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(CopyConstructor);
		PARSE_DECL();

		~CopyConstructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Copy Constructor";
		}

		[[nodiscard]]
		base::Optional<base::StrID> getInternalSymbolName() const final {
			return base::StrID("copy");
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
