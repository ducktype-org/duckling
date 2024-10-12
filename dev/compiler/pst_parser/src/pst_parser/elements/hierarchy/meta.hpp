#pragma once

#include "../../lang_parser_state.hpp"
#include "../elements_common.hpp"

#include <diagnostic/source_position.hpp>
#include <token_parser_core/token_stream.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/automatic.hpp>

#include <base/string_id.hpp>

#include <unicode/unistr.h>

#include <set>

namespace pst {
	class PstStmtVisitor;
	class Attribute;

	using StateCondition = bool(const LangParserState&, i64);

	using GetName = std::string (*)();

	enum class StmtKind {
		Import,
		Using,
		Alias,
		Fun,
		Namespace,
		CodeDecl,
		Action,
		ExprStmt,
		Class,
		TopLevel,
		Const,
		Variable,
		// Class Statements
		Method,
		Field,
		Constructor,
		Destructor,
		AccessBlock,
	};

	class Stmt: public LangElement {
		StmtKind kind;

	protected:
		using AttrList = std::vector<ParserRef<Attribute>>;

		AttrList attributes;

		Stmt(StmtKind kind, const dia::SourcePosition& position):
			  LangElement(position),
			  kind(kind) {}

		static AttrList collectAttributes(LangParserState& state);

		/**
		 * @brief Prepends attributes after parsing handling sub elements and position.
		 */
		void addAttributes(std::vector<ParserRef<Attribute>>&& additions);

		void dprintAttributes(std::ostream& out) const;

		void dprintPrefix(std::ostream& out) const override;

	public:
		[[nodiscard]]
		StmtKind getKind() const {
			return kind;
		}

		static ParserRef<Stmt> parse(LangParserState& state);
		bool                   trailingSemicolon() override;
		virtual void           acceptVisitor(PstStmtVisitor& visitor) const = 0;

		/**
		 * @note This might need to return a vector of borrow pointers instead
		 */
		[[nodiscard]]
		auto& getAttributes() const {
			return attributes;
		}

		[[nodiscard]]
		bool isStatement() const final {
			return true;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Statement";
		}

		/**
		 * @brief Determines if given statement is a declaration.
		 * Declaration is everything that is considered a unique symbol in HELIOS.
		 * For example declarations are:
		 * * functions
		 * * classes
		 * * aliases and usings
		 * * ifs, whiles with a name
		 * * variable declaration
		 *
		 * For example declarations are not:
		 * * expressions
		 * * ifs, whiles without name
		 * * return, break
		 *
		 * @note: this definition of declaration might not
		 * always be equivalent to intuitive thinking about declarations.
		 */
		[[nodiscard]]
		virtual bool isDeclaration() const {
			return false;
		}
	};

#define STMT_CHILD_CONSTRUCTOR(class_name) \
	class_name(const dia::SourcePosition& position): Stmt(StmtKind::class_name, position) {}

	class NotStmt: public LangElement {
	public:
		explicit NotStmt(const dia::SourcePosition& position): LangElement(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Not Statement";
		}

		bool trailingSemicolon() override;
	};

	struct ClassContext {
		base::StrID                                 name;
		std::vector<base::c_borrow_ptr<tpc::Token>> specifiers;
	};

	class ClassStmt: public Stmt {
	protected:
		inline static const std::set<lang_def::Keyword> class_specs = {
			lang_def::Keyword::Public,
			lang_def::Keyword::Private,
			lang_def::Keyword::Protected,
			lang_def::Keyword::Static,
		};
		ClassContext context;

		void parseSpecifiers(LangParserState& state);

		[[nodiscard]]
		static i64 countSpecifiers(LangParserState& state);

		void dprintPrefix(std::ostream& out) const override;

		ClassStmt(StmtKind kind, const dia::SourcePosition& pos, ClassContext ctx):
			  Stmt(kind, pos),
			  context(std::move(ctx)) {}

	private:
		static ParserRef<ClassStmt> chooseStmt(LangParserState& state, const ClassContext& ctx);

	public:
		static ParserRef<ClassStmt> parse(LangParserState& state, const ClassContext& ctx);

		const ClassContext& getContext() { return { context }; }

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Element";
		}
	};

}
