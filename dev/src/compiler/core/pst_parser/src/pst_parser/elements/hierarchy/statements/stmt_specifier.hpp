#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief A simple statement specifier (ex. public / private)
	 *
	 * Allows a limited set of keywords.
	 * Examples:
	 *  - public fun ...
	 *  - public { stmt1; stmt2; }
	 */
	class StmtSpecifier final: public Stmt {
		Keyword specifier = Keyword::NotAKeyword;
		NAMED_CHILD(code_block_or_stmt, CodeBlockOrStmt);

	public:
		static constexpr std::array<Keyword, 3> SPECIFIERS_ARRAY = {
			Keyword::Public,
			Keyword::Private,
			Keyword::Protected,
		};
		static const std::set<Keyword> SPECIFIERS;

		STMT_CHILD_CONSTRUCTOR(StmtSpecifier, ElementKind::StmtSpecifier);
		static MBox<StmtSpecifier> parse(LangParserState& state);

		~StmtSpecifier() final = default;
		void dprint(std::ostream& out) const final;
		bool trailingSemicolon() override;
		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "StmtSpecifier";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}
	};
}
