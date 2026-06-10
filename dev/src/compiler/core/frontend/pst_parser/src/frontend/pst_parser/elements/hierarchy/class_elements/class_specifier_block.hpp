#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Access specifier block inside of a class.
	 *
	 * They are used to change the visibility of multiple definitions in a class
	 */
	class ClassSpecifierBlock final: public ClassStmt {
		PARENT_CLASS(ClassStmt);
		THIS_CLASS(ClassSpecifierBlock);
		CLONE_SUBELEMENTS();
		CLONE_SIGNATURE_DEFAULT_OVERRIDE();
	protected:
		NAMED_CHILD(block, ClassBlock);

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		ELEMENT_CLONE_DECL(ClassSpecifierBlock);
		CLASS_STMT_CHILD_CONSTRUCTOR(ClassSpecifierBlock, ElementKind::ClassSpecifierBlock);
		CLASS_STMT_PARSE(ClassSpecifierBlock);

		~ClassSpecifierBlock() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class specifier block";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}

		[[nodiscard]]
		AccessLocked<ClassBlock> getBlock() const {
			return block.give();
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return false;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
