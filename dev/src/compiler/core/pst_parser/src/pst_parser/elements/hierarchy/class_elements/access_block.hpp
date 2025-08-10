#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Access specifier block inside of a class.
	 *
	 * They are used to change the visibility of multiple definitions in a class
	 */
	class AccessBlock final: public ClassStmt {
		static inline const std::set<lang_def::Keyword> ACCESS_SPECIFIERS = {
			lang_def::Keyword::Public,
			lang_def::Keyword::Private,
			lang_def::Keyword::Protected,
		};

		lang_def::Keyword specifier = lang_def::Keyword::NotAKeyword;
		NAMED_CHILD(block, ClassBlock);

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(AccessBlock, ElementKind::AccessBlock);
		CLASS_STMT_PARSE(AccessBlock);

		~AccessBlock() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Access specification block";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::None;
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
