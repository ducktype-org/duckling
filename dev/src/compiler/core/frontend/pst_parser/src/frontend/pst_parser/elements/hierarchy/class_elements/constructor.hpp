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
		THIS_CLASS(Constructor);
		PARENT_CLASS(ClassSpecial);
		CLONE_SUBELEMENTS();
		CLONE_SIGNATURE_DEFAULT_OVERRIDE();
	protected:

		NAMED_CHILD_OPT(ident, IdentifierWrapper);  ///< If no value it's "create" is implied
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD(inits, InitList);
		NAMED_CHILD(body, CodeBlock);

	public:
		ELEMENT_CLONE_DECL(Constructor);
		CLASS_STMT_SPEC_CONSTRUCTOR(Constructor);
		CLASS_STMT_PARSE(Constructor);

		~Constructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Constructor";
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getIdentifier() const {
			return ident.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]]
		base::Optional<base::StrID> getInternalSymbolName() const final {
			if (ident)
				return ident->internal()->unwrap();
			else
				return base::StrID("create");
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return ident.map([](const auto& acc) { return acc.give(); });
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
