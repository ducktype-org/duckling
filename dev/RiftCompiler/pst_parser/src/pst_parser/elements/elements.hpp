#pragma once

#include "../rift_parser_state.hpp"
#include "base/unique_pointer.hpp"
#include "diagnostic/source_position.hpp"
#include "elements_common.hpp"

#include <token_parser_core/token_stream.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/automatic.hpp>

#include <base/string_id.hpp>

#include <unicode/unistr.h>
#include <variant>

// @TODO: make generic optional

namespace pst {

	class Expr;

	class PstStmtVisitor;

	enum class StmtKind {
		Attribute,
		Import,
		Using,
		Alias,
		Fun,
		Namespace,
		CodeDecl,
		Action,
		Expr,
		Struct,
		TopLevel,
		Const,
		Variable
	};

	class Stmt: public RiftElement {
		StmtKind kind;

	protected:
		Stmt(StmtKind kind, const dia::SourcePosition& position):
			  RiftElement(position),
			  kind(kind) {}

	public:
		[[nodiscard]]
		StmtKind getKind() const {
			return kind;
		}

		static ParserRef<Stmt> parse(RiftParserState& state);
		bool                   trailingSemicolon() override;
		virtual void           acceptVisitor(PstStmtVisitor& visitor) const = 0;

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

	class NotStmt: public RiftElement {
	public:
		explicit NotStmt(const dia::SourcePosition& position): RiftElement(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Not Statement";
		}

		bool trailingSemicolon() override;
	};

	using StateCondition = bool(const RiftParserState&, i64);

	using GetName = std::string();

	/**
	 * @brief General Element representing a list of Elements.
	 *
	 * @tparam SubElements - Kept Elements, has to have precise length parse like Expr
	 * @tparam NON_EMPTY - Should empty list be an error.
	 * @tparam BRACKETS - expected brackets or None if not expected
	 * @tparam isSeparator - Separator should always be skip-able with one skip.
	 * @tparam isEnding - Check for successful ending.
	 * @tparam getName - List name getter for errors.
	 * @tparam Container - Vector-like container of SubElements with emplace_back. Possibly with
	 * other condition because of iteration.
	 */
	template<
		class SubElements,
		bool                      NON_EMPTY,
		lexer::Token::BracketType BRACKETS,
		StateCondition            isSeparator,
		StateCondition            isEnding,
		GetName                   getName,
		class Container = std::vector<ParserRef<SubElements>>>
	class List final: public NotStmt {
		Container elements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(elements, SubElements)

		explicit List(const dia::SourcePosition& position): NotStmt(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return getName() + " list";
		}

		static ParserRef<List> parse(RiftParserState& state);

		void dprint(std::ostream& out) const final {
			out << "{\"List\" : [";
			for (auto& x: elements) {
				tpc::nullAwareDprint(x, out);
				out << ",";
			}
			out << "]}";
		}

		~List() final = default;
	};

	using ParamList = List<
		Expr,
		false,
		lexer::Token::BracketType::Round,
		detail::Conditions::isComma,
		detail::Conditions::isSentinel,
		detail::NameGetters::parameterList>;

	using RetList = List<
		Expr,
		true,
		lexer::Token::BracketType::None,
		detail::Conditions::isComma,
		detail::Conditions::isCurlyGroup,
		detail::NameGetters::returnList>;

	using InheritList = List<
		Expr,
		true,
		lexer::Token::BracketType::None,
		detail::Conditions::isComma,
		detail::Conditions::isCurlyGroup,
		detail::NameGetters::inheritanceList>;

	using AtrArgList = List<
		Expr,
		false,
		lexer::Token::BracketType::Round,
		detail::Conditions::isComma,
		detail::Conditions::isSentinel,
		detail::NameGetters::attributeArgList>;

	class DottedName final: public NotStmt {
		std::vector<tpc::Identifier> names;
		bool                         star = false;

	public:
		[[nodiscard]]
		auto begin() const {
			return names.cbegin();
		}

		[[nodiscard]]
		auto end() const {
			return names.cend();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Dotted Name";
		}

		explicit DottedName(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<DottedName> parse(RiftParserState& state);

		[[nodiscard]]
		std::vector<base::StrId> getNames() const;
		[[nodiscard]]
		bool getStar() const;

		void dprint(std::ostream& out) const final;
		~DottedName() final = default;
	};

	class Attribute final: public Stmt {
		tpc::Identifier       name;
		ParserRef<AtrArgList> args = nullptr;

	public:
		STMT_CHILD_CONSTRUCTOR(Attribute);
		static ParserRef<Attribute> parse(RiftParserState& state);
		~Attribute() final = default;

		void dprint(std::ostream& out) const final;
		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Attribute";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @note: Import allows for two syntaxes right now:
	 * import A.B as D;
	 * import A.B.* as D;
	 *
	 * the optional "star" is ignored.
	 */
	class Import final: public Stmt {
		ParserRef<DottedName> names;
		tpc::Identifier       alias;

	public:
		STMT_CHILD_CONSTRUCTOR(Import);
		static ParserRef<Import> parse(RiftParserState& state);
		[[nodiscard]]
		const decltype(names)& getNames() const;

		[[nodiscard]]
		base::StrId getAlias() const {
			return alias.value;
		}

		/**
		 * @note In the future this functionality will be done by HELIOS.
		 * This functionality is needed to implement early import system for testing.
		 */
		[[nodiscard]]
		std::vector<base::StrId> getModulePath() const;

		[[nodiscard]]
		bool getStar() const;
		~Import() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Import";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	class Using final: public Stmt {
		ParserRef<DottedName> names;

	public:
		STMT_CHILD_CONSTRUCTOR(Using);
		static ParserRef<Using> parse(RiftParserState& state);

		[[nodiscard]]
		auto getPointed() const {
			return names->getNames();
		}

		[[nodiscard]]
		bool isStar() const {
			return names->getStar();
		}

		~Using() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Using";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	class Alias final: public Stmt {
		tpc::Identifier       name;
		ParserRef<DottedName> points_to;

	public:
		STMT_CHILD_CONSTRUCTOR(Alias);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		auto getPointed() const {
			return points_to->getNames();
		}

		static ParserRef<Alias> parse(RiftParserState& state);
		~Alias() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Alias";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	class CodeBlock final: public NotStmt {
		std::vector<ParserRef<Stmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, Stmt)

		explicit CodeBlock(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<CodeBlock> parse(RiftParserState& state);
		~CodeBlock() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}
	};

	class CodeBlockOrStmt final: public NotStmt {
		std::variant<ParserRef<Stmt>, ParserRef<CodeBlock>> content;

	public:
		explicit CodeBlockOrStmt(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<CodeBlockOrStmt> parse(RiftParserState& state);
		~CodeBlockOrStmt() final = default;
		void dprint(std::ostream& out) const final;

		using const_iterator = CodeBlock::const_iterator;
		[[nodiscard]]
		const_iterator begin() const;
		[[nodiscard]]
		const_iterator end() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block or Statement";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}
	};

	class RoundGroupExpr final: public NotStmt {
		ParserRef<Expr> expr = nullptr;

	public:
		explicit RoundGroupExpr(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<RoundGroupExpr> parse(RiftParserState& state);
		~RoundGroupExpr() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Round Group Expression";
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getExpr() const {
			return expr.borrow();
		}
	};

	class EmptyExprError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a non-empty expression.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyExprError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	/**
	 * @TODO: improve comma separated expressions and expression parse options in general.
	 */
	class Expr final: public Stmt {
	public:
		enum class GroupType {
			RoundGroup  = 0,
			SquareGroup = 1,
			CurlyGroup  = 2,
			AngleGroup  = 3,
		};

		struct Group;
		/** @brief Represents multiple comma separated expressions.
		 * For example `a, b` in `a, b = c` or `b, c` in `a = (b, c)`
		 */
		struct CommaSeparated;
		struct Operator;
		struct Identifier;
		struct NumLiteral;
		struct Block;

		struct KeywordValue {
			rift_def::Keyword keyword;
		};

		using ExprElem = std::
			variant<Operator, Identifier, NumLiteral, Group, KeywordValue, CommaSeparated, Block>;

		struct CommaSeparated {
			std::vector<ParserRef<Expr>> expr;
		};

		struct Group {
			GroupType       type;
			ParserRef<Expr> expr;
		};

		struct Block {
			ParserRef<CodeBlock> block;
		};

		struct Operator {
			base::StrId oper_id;
		};

		struct Identifier {
			base::StrId indent_id;
		};

		struct NumLiteral {
			base::StrId num_id;
		};

		std::vector<ExprElem> elements;

		STMT_CHILD_CONSTRUCTOR(Expr);
		/**
		 * @brief parses the expression until its over
		 * @param allow_comma whether the expression can be a set of comma separated expressions.
		 */
		static ParserRef<Expr> parse(RiftParserState& state, bool allow_comma = false);

		[[nodiscard]]
		std::string elementType() const override {
			return "Expression";
		}

		/**
		 * @brief parses the expression until a condition is met or end of token stream.
		 *
		 * @tparam until Condition to end the parsing.
		 * @tparam positiveEnd Condition for positive parsing end.
		 * @tparam badEndMessage Message if the @p positiveEnd condition is not met.
		 * @param allow_comma whether the expression can be a set of comma separated expressions.
		 *
		 * @return ParserRef<Expr>
		 */
		template<
			StateCondition                  until,
			StateCondition                  positiveEnd,
			std::derived_from<dia::Message> badEndMessage>
		requires std::constructible_from<badEndMessage, dia::SourcePosition>
		static ParserRef<Expr> parseUntil(RiftParserState& state, bool allow_comma = false) {
			// look ahead:
			usize count = 0;
			while (!until(state, (i64) count) && count < state.ctokens().size()) count++;

			if (!positiveEnd(state, (i64) count)) {
				// Handle negative end:
				auto pos = state.getPosition(-1);
				if (count != 0) {
					pos = state.getPosition(0, (i64) count - 1);
				} else if (!state.isEOF()) {
					auto other = state.getPosition(0);
					pos        = dia::SourcePosition(pos, other.getStart());
				}
				state.log(base::make_unique<badEndMessage>(pos));
			} else if (count == 0) {
				// Handle empty expression:
				auto pos = state.getPosition(-1);
				if (!state.isEOF()) {
					auto other = state.getPosition(0);
					pos        = dia::SourcePosition(pos, other.getStart());
				}
				state.log(base::make_unique<EmptyExprError>(pos));
			}

			// Don't parse empty expression:
			if (count == 0) return nullptr;

			return Expr::parse(state, count, true, allow_comma);
		}

		/**
		 * @p exact_len = false: parses the expression until its over or until it parses @p len
		 * tokens
		 * @p exact_len = true: parses the expression until it parses @p len tokens
		 * @param allow_comma whether the expression can be a set of comma separated expressions.
		 */
		static ParserRef<Expr> parse(
			RiftParserState& state, usize len, bool exact_len = true, bool allow_comma = false
		);
		void dprint(std::ostream& out) const final;
		~Expr() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @note Action assumes optional expression before the semicolon.
	 */
	class Action: public Stmt {
	protected:
		base::Optional<ParserRef<Expr>> expr;

	public:
		STMT_CHILD_CONSTRUCTOR(Action);
		static ParserRef<Action> parse(RiftParserState& state);
		~Action() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Action";
		}

		[[nodiscard]]
		base::Optional<ParserCBorrowRef<Expr>> getValue() const {
			return expr.map([](const auto& e) { return e.borrow(); });
		}
	};

	class Return final: public Action {
	public:
		explicit Return(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Return() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Break final: public Action {
	public:
		explicit Break(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Continue final: public Action {
	public:
		explicit Continue(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Continue() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Redo final: public Action {
	public:
		explicit Redo(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Redo() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Restart final: public Action {
	public:
		explicit Restart(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Restart() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Defer final: public Action {
	public:
		explicit Defer(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Defer() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @todo Should throw be an action?
	 */
	class Throw final: public Action {
	public:
		explicit Throw(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Throw() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	// TODO: Merge it with variable. Or perhaps make a new class DataStorage.
	class Const final: public Stmt {
		tpc::Identifier name;
		ParserRef<Expr> type;
		ParserRef<Expr> value;

	public:
		STMT_CHILD_CONSTRUCTOR(Const);
		static ParserRef<Const> parse(RiftParserState& state);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getType() const {
			return type.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getValue() const {
			return value.borrow();
		}

		~Const() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Const";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	/**
	 * @todo should assert be an action
	 */
	class Decl: public Stmt {
	public:
		Decl(StmtKind kind, const dia::SourcePosition& position): Stmt(kind, position) {}

		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Declaration";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}
	};

#define DECL_CHILD_CONSTRUCTOR(class_name) \
	class_name(const dia::SourcePosition& position): Decl(StmtKind::class_name, position) {}

	class CodeDecl: public Decl {
	public:
		DECL_CHILD_CONSTRUCTOR(CodeDecl);

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Declaration";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return false;
		}
	};

	class TopLevel final: public Decl {
		std::vector<tpc::ParserRef<Stmt>> statements;

	public:
		DECL_CHILD_CONSTRUCTOR(TopLevel);
		static ParserRef<TopLevel> parse(RiftParserState& state);

		~TopLevel() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Top Level";
		}

		[[nodiscard]]
		const auto& getStatements() const {
			return statements;
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Block final: public CodeDecl {
		tpc::OptionalIdentifier optional_name;
		ParserRef<CodeBlock>    code_block = nullptr;

	public:
		explicit Block(const dia::SourcePosition& position): CodeDecl(position) {}

		static ParserRef<Block> parse(RiftParserState& state);
		~Block() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Block";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Namespace final: public Decl {
		tpc::Identifier      name;
		ParserRef<CodeBlock> body = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Namespace);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		ParserCBorrowRef<CodeBlock> getBody() const {
			return body.borrow();
		}

		static ParserRef<Namespace> parse(RiftParserState& state);
		~Namespace() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Namespace";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @note outdated with "current" syntax (one inherits then implemnets etc)
	 */
	class Struct final: public Decl {
		tpc::Identifier        name;
		ParserRef<InheritList> bases = nullptr;
		ParserRef<CodeBlock>   body  = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Struct);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		ParserCBorrowRef<CodeBlock> getBody() const {
			return body.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<InheritList> getBases() const {
			return bases.borrow();
		}

		static ParserRef<Struct> parse(RiftParserState& state);
		~Struct() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Struct";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Fun final: public Decl {
		tpc::Identifier            name;
		ParserRef<ParamList>       params = nullptr;
		ParserRef<RetList>         rets   = nullptr;
		ParserRef<CodeBlockOrStmt> body   = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Fun);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		auto getParams() const {
			return params.borrow();
		}

		[[nodiscard]]
		auto getBody() const {
			return body.borrow();
		}

		static ParserRef<Fun> parse(RiftParserState& state);
		void                  dprint(std::ostream& out) const final;
		~Fun() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Variable final: public Decl {
		tpc::Identifier name;
		ParserRef<Expr> type     = nullptr;
		ParserRef<Expr> value    = nullptr;
		bool            is_const = true;

	public:
		DECL_CHILD_CONSTRUCTOR(Variable);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		bool trailingSemicolon() override;

		[[nodiscard]]
		ParserCBorrowRef<Expr> getType() const {
			return type.borrow();
		}

		static ParserRef<Variable> parse(RiftParserState& state);
		void                       dprint(std::ostream& out) const override;
		~Variable() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Variable";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class If final: public CodeDecl {
		ParserRef<RoundGroupExpr>  condition = nullptr;
		tpc::OptionalIdentifier    optional_name;
		ParserRef<CodeBlockOrStmt> body = nullptr;

		// @TODO: else

	public:
		explicit If(const dia::SourcePosition& position): CodeDecl(position) {}

		static ParserRef<If> parse(RiftParserState& state);
		void                 dprint(std::ostream& out) const final;
		~If() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "If";
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getCondition() const {
			return condition->getExpr();
		}

		[[nodiscard]]
		ParserCBorrowRef<CodeBlockOrStmt> getBody() const {
			return body.borrow();
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class While final: public CodeDecl {
		ParserRef<RoundGroupExpr>  condition = nullptr;
		tpc::OptionalIdentifier    optional_name;
		ParserRef<CodeBlockOrStmt> body = nullptr;

	public:
		explicit While(const dia::SourcePosition& position): CodeDecl(position) {}

		static ParserRef<While> parse(RiftParserState& state);
		void                    dprint(std::ostream& out) const final;
		~While() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "While";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @brief Class designated to be the parent of non-functional statements that are only used
	 * internally for testing.
	 */
	class RiftTestingStmt: public Stmt {
	public:
		RiftTestingStmt(StmtKind kind, const dia::SourcePosition& position): Stmt(kind, position) {}
	};

#define RIFT_TEST_CHILD_CONSTRUCTOR(class_name)      \
	class_name(const dia::SourcePosition& position): \
		  RiftTestingStmt(StmtKind::class_name, position) {}

	template<GetName type>
	class OpeningBracketMissingError final: public dia::Error {
	private:
		lexer::Token::BracketType bracket;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			std::string str_bracket{};
			icu::UnicodeString(bracket).toUTF8String(str_bracket);
			std::stringstream ss;
			ss << "Opening bracket " << str_bracket << " of a " << type()
			   << " list expected after here.";
			return ss.str();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		OpeningBracketMissingError(dia::SourcePosition pos, lexer::Token::BracketType bracket):
			  dia::Error(pos),
			  bracket(bracket) {}
	};

	template<GetName type>
	class EmptyListError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "This " + type() + " list shouldn't be empty.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyListError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<GetName type>
	class EmptyListElementError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "This " + type() + " list element shouldn't be empty.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyListElementError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<GetName type>
	class EmptyFieldError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Empty field in the " + type() + " list.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		EmptyFieldError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	template<GetName type>
	class NoSeparatorError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return type() + " list separator expected.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NoSeparatorError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	/**
	 * @note We might want to move it outside and allow only for specific instances to be cleaner.
	 */
	template<
		class SubElements,
		bool                      NON_EMPTY,
		lexer::Token::BracketType BRACKETS,
		StateCondition            isSeparator,
		StateCondition            isEnding,
		GetName                   getName,
		class Container>
	auto List<SubElements, NON_EMPTY, BRACKETS, isSeparator, isEnding, getName, Container>::parse(
		RiftParserState& state
	) -> ParserRef<List> {
		auto position = state.getPosition();

		auto out = tpc::makeRef<List>(position);

		// Handle opening brackets:
		if constexpr (BRACKETS != lexer::Token::BracketType::None) {
			if (!state[0].isBracketGroup(BRACKETS)) {
				state.log(base::make_unique<OpeningBracketMissingError<getName>>(
					state.getPosition(-1), BRACKETS
				));
				return nullptr;
			}
			state.parse(out).goDown();
		}

		usize expr_length{};
		if (state.empty() || isEnding(state, 0)) {
			// Handle empty expression
			if constexpr (NON_EMPTY)
				state.log(base::make_unique<EmptyListError<getName>>(state.getPosition(-1)));
		} else {
			while (true) {
				expr_length = 0;

				// Find next separator or end
				while (!state[(i64) expr_length].is(lexer::Token::Type::Sentinel)
				       && !isSeparator(state, (i64) expr_length)
				       && !isEnding(state, (i64) expr_length)) {
					expr_length++;
				}
				if (expr_length == 0) {
					// Handle empty field errors with sensible ranges
					if (state.empty() || isEnding(state, 0)) {
						auto pos = state.getPosition(-1);
						if (!state.isEOF()) {
							auto other = state.getPosition();
							pos        = dia::SourcePosition(pos, other.getStart());
						}
						state.log(base::make_unique<EmptyFieldError<getName>>(pos));
						break;
					} else {
						state.log(
							base::make_unique<EmptyFieldError<getName>>(state.getPosition(-1, 0))
						);
						state.parse(out).eatOne();
						continue;
					}
				}

				ParserRef<SubElements> ref;
				state.parse(out).template with<SubElements>(
					&ref, SubElements::parse, (usize) expr_length, true, false
				);
				out->elements.emplace_back(std::move(ref));

				if (isEnding(state, 0)) break;
				if (isSeparator(state, 0))
					state.parse(out).eatOne();
				else
					state.log(base::make_unique<NoSeparatorError<getName>>(state.getPosition()));
			}
		}

		// Handle closing brackets
		if constexpr (BRACKETS != lexer::Token::BracketType::None) state.parse(out).goUpAndSkip();

		return out;
	}
}
