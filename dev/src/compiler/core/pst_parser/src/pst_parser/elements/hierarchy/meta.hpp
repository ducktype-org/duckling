#pragma once

#include "../../lang_parser_element.hpp"
#include "../../pst_state_forward.hpp"
#include "../elements_common.hpp"

#include <base/string_id.hpp>

#include <diagnostic/source_position.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>

#include <set>

namespace pst {
	class Attribute;

	using StateCondition = bool(const LangParserState&, i64);

	using GetName = std::string (*)();

	/**
	 * @brief A general element that is a common ancestor for elements that aren't statements
	 */
	class NotStmt: public LangElement {
	public:
		explicit NotStmt(const dia::SourcePosition& position): LangElement(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Not Statement";
		}

		bool trailingSemicolon() override;
	};

	/**
	 * @brief Three declaration options:
	 *
	 * * None - This statement doesn't introduce any symbols. For example an expression statement or
	 * a return statement.
	 * * Symbol - This statement introduces a symbol. For example a function
	 * declaration, import, using and variable declaration.
	 * * Transparent - This statement contains or
	 * links somewhere where there might be introduced. For example a macro expansion or a specifier
	 * block.
	 */
	enum class DeclKind {
		None,
		Symbol,
		Transparent,
	};

	enum class StmtKind {
		Import,
		Using,
		Alias,
		Fun,
		FunDecl,
		Namespace,
		CodeDecl,
		StmtSpecifier,
		Action,
		ExprStmt,
		Class,
		TopLevel,
		Const,
		Variable,
		Expand,
		// Class Statements
		Method,
		Field,
		Constructor,
		CopyConstructor,
		Destructor,
		AccessBlock,
		NonClassStmt
	};

	/**
	 * @brief A general element that is a common ancestor of all statements.
	 */
	class Stmt: public LangElement {
		StmtKind kind;

	protected:
		using AttrList    = std::vector<AccessInternalAnonymous<Attribute>>;
		using AttrBoxList = std::vector<Box<Attribute>>;

		AttrList attributes;

		Stmt(StmtKind kind, const dia::SourcePosition& position):
			  LangElement(position),
			  kind(kind) {}

		static AttrBoxList collectAttributes(LangParserState& state);

		/**
		 * @brief Prepends attributes after parsing handling sub elements and position.
		 */
		void addAttributes(LangParserState& state, AttrBoxList&& additions);

		void dprintAttributes(std::ostream& out) const;

		void dprintPrefix(std::ostream& out) const override;

		void calcElementPathsRecursive() override;

	public:
		[[nodiscard]]
		StmtKind getStmtKind() const {
			return kind;
		}

		static MBox<Stmt> parse(LangParserState& state);
		bool              trailingSemicolon() override;
		void              acceptVisitor(PstVisitor& visitor) const override = 0;

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
		virtual DeclKind isDeclaration() const {
			return DeclKind::None;
		}

		/**
		 * @brief Get the symbol name declared by a given statement if it exists.
		 */
		[[nodiscard]]
		virtual base::Optional<base::StrID> getDeclSymbolName() const {
			return {};
		}
	};

#define STMT_CHILD_CONSTRUCTOR(class_name, element_kind_)                                   \
	class_name(const dia::SourcePosition& position): Stmt(StmtKind::class_name, position) { \
		this->element_kind = element_kind_;                                                 \
	}

	/**
	 * @brief Context needed in class parsing
	 *
	 * includes:
	 *  - name - class name
	 *  - specifiers - current access and other specifiers
	 */
	struct ClassContext {
		base::StrID                     name;
		std::vector<CRef<lexer::Token>> specifiers;
	};

	/**
	 * @brief Statements specific to the inside of a class
	 */
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
		static MBox<ClassStmt> chooseStmt(LangParserState& state, const ClassContext& ctx);

	public:
		static MBox<ClassStmt> parse(LangParserState& state, const ClassContext& ctx);

		const ClassContext& getContext() { return { context }; }

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Element";
		}
	};

}
