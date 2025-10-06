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
	 *  - extern ("C") {...}
..	 */
	class StmtSpecifier final: public Stmt {
		Keyword specifier = Keyword::NotAKeyword;
		NAMED_CHILD(code_block_or_stmt, CodeBlockOrStmt);
		NAMED_CHILD_OPT(call_list, CallList);

	public:
		static constexpr std::array SPECIFIERS_ARRAY
			= { Keyword::Public, Keyword::Private, Keyword::Protected,
			    Keyword::Extern, Keyword::Test,    Keyword::Debug };

		static constexpr std::array SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY = {
			Keyword::Extern,
		};

		static const std::set<Keyword> SPECIFIERS;
		static const std::set<Keyword> SPECIFIEIRS_CALL_LIST_REQUIRED;

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

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getContent() const {
			return code_block_or_stmt.give();
		}

		[[nodiscard]]
		Keyword getSpecifier() const {
			return specifier;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<CallList>> getArgs() const {
			if (call_list.has_value())
				return call_list->give();
			else
				return {};
		}
	};
}
