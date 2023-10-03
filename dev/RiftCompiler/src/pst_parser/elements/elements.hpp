#pragma once

#include "../rift_parser_base.hpp"

#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/token_stream.hpp>

#include <base/string_id.hpp>

#include <iostream>
#include <span>
#include <variant>

// @TODO: AttrList

// @TODO: make generic optional

namespace pst {

	class Expr;

	struct DottedName {
		std::vector<tpc::Identifier> names;
		bool                         star = false;
		[[nodiscard]]
		std::vector<base::StrId> getNames() const;
	};

	void parseDottedName(tpc::ParserState& state, DottedName* d_name);

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

		EagerLookup,
	};

	class Stmt: public RiftElement {
		StmtKind kind;

	protected:
		Stmt(StmtKind kind, lexer::SourcePosition position):
			  RiftElement(std::move(position)),
			  kind(kind) {}

	public:
		[[nodiscard]]
		StmtKind getKind() const {
			return kind;
		}

		static ParserRef<Stmt> parse(RiftParserState& state);
		bool                   trailingSemicolon() override;
	};

#define STMT_CHILD_CONSTRUCTOR(class_name) \
	class_name(lexer::SourcePosition position): Stmt(StmtKind::class_name, std::move(position)) {}

	class NotStmt: public RiftElement {
		// @TODO:

	public:
		explicit NotStmt(lexer::SourcePosition position): RiftElement(std::move(position)) {}

		bool trailingSemicolon() override;
	};

	class ParamList: public NotStmt {
		std::vector<ParserRef<Expr>> params;

	public:
		explicit ParamList(lexer::SourcePosition position): NotStmt(std::move(position)) {}

		static ParserRef<ParamList> parse(RiftParserState& state);
		virtual void                dprint(std::ostream& out) const final;
		virtual ~ParamList() = default;
	};

	class RetList: public NotStmt {
		std::vector<ParserRef<Expr>> rets;

	public:
		explicit RetList(lexer::SourcePosition position): NotStmt(std::move(position)) {}

		static ParserRef<RetList> parse(RiftParserState& state);
		virtual void              dprint(std::ostream& out) const final;
		virtual ~RetList() = default;
	};

	class ArgList: public NotStmt {
		// @TODO

	public:
		explicit ArgList(lexer::SourcePosition position): NotStmt(std::move(position)) {}

		static ParserRef<ArgList> parse(RiftParserState& state);
		virtual ~ArgList() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class Attribute: public Stmt {
		tpc::Identifier    name;
		ParserRef<ArgList> args = nullptr;

	public:
		STMT_CHILD_CONSTRUCTOR(Attribute);
		static ParserRef<Attribute> parse(RiftParserState& state);
		virtual ~Attribute() = default;
		virtual void dprint(std::ostream& out) const final;
		bool         trailingSemicolon() override;
	};

	class Import: public Stmt {
		DottedName names;

	public:
		STMT_CHILD_CONSTRUCTOR(Import);
		static ParserRef<Import> parse(RiftParserState& state);
		const decltype(names)&   getNames() const;
		bool                     getStar() const;
		virtual ~Import() = default;
		virtual void dprint(std::ostream& out) const final;

		// @TODO:
	};

	class Using: public Stmt {
		DottedName names;

	public:
		STMT_CHILD_CONSTRUCTOR(Using);
		static ParserRef<Using> parse(RiftParserState& state);

		auto getPointed() const { return names.getNames(); }

		bool isStar() const { return names.star; }

		virtual ~Using() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class Alias: public Stmt {
		tpc::Identifier name;
		DottedName      points_to;

	public:
		STMT_CHILD_CONSTRUCTOR(Alias);

		base::StrId getName() const { return name.value; }

		auto getPointed() const { return points_to.getNames(); }

		static ParserRef<Alias> parse(RiftParserState& state);
		virtual ~Alias() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class CodeBlock: public NotStmt {
		std::vector<ParserRef<Stmt>> statements;

	public:
		explicit CodeBlock(lexer::SourcePosition position): NotStmt(std::move(position)) {}

		static ParserRef<CodeBlock> parse(RiftParserState& state);
		virtual ~CodeBlock() = default;
		virtual void dprint(std::ostream& out) const final;

		std::span<const ParserRef<Stmt>> getStatements() const { return statements; }
	};

	class CodeBlockOrStmt: public NotStmt {
		std::variant<ParserRef<Stmt>, ParserRef<CodeBlock>> content;

	public:
		explicit CodeBlockOrStmt(lexer::SourcePosition position): NotStmt(std::move(position)) {}

		static ParserRef<CodeBlockOrStmt> parse(RiftParserState& state);
		virtual ~CodeBlockOrStmt() = default;
		virtual void dprint(std::ostream& out) const final;

		std::span<const ParserRef<Stmt>> getStatements() const;
	};

	class RoundGroupExpr: public NotStmt {
		ParserRef<Expr> expr = nullptr;

	public:
		explicit RoundGroupExpr(lexer::SourcePosition position): NotStmt(std::move(position)) {}

		static ParserRef<RoundGroupExpr> parse(RiftParserState& state);
		virtual ~RoundGroupExpr() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class Expr: public Stmt {
	public:
		enum class GroupType {
			RoundGroup  = 0,
			SquareGroup = 1,
			CurlyGroup  = 2,
			AngleGroup  = 3,
		};

	private:
		// @TODO: change StrId to Operator::, Keyword::, etc
		struct Group;
		struct Operator;
		struct Identifier;
		struct NumLiteral;

		struct KeywordValue {
			base::StrId key_id;
		};

		typedef std::variant<Operator, Identifier, NumLiteral, Group, KeywordValue> ExprElem;

		struct Group {
			GroupType       type;
			ParserRef<Expr> expr;
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

	public:
		STMT_CHILD_CONSTRUCTOR(Expr);
		/**
		 * @brief parses the expression until its over
		 */
		static ParserRef<Expr> parse(RiftParserState& state);
		/**
		 * @p exact_len = false: parses the expression until its over or until it parses @p len
		 * tokens
		 * @p exact_len = true: parses the expression until it parses @p len tokens
		 */
		static ParserRef<Expr> parse(RiftParserState& state, usize len, bool exact_len = true);
		void                   dprint(std::ostream& out) const final;
		virtual ~Expr() = default;
	};

	class Action: public Stmt {
	protected:
		std::optional<ParserRef<Expr>> expr;

	public:
		STMT_CHILD_CONSTRUCTOR(Action);
		// @TODO: do different Actions than ones with 0 or 1 expressions following exist?
		static ParserRef<Action> parse(RiftParserState& state);
		virtual ~Action() = default;

		// TODO:
	};

	class Return: public Action {
	public:
		explicit Return(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Return() = default;
	};

	class Break: public Action {
	public:
		explicit Break(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Break() = default;
	};

	class Continue: public Action {
	public:
		explicit Continue(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Continue() = default;
	};

	class Redo: public Action {
	public:
		explicit Redo(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Redo() = default;
	};

	class Restart: public Action {
	public:
		explicit Restart(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Restart() = default;
	};

	class Defer: public Action {
	public:
		explicit Defer(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Defer() = default;
	};

	// @TODO: should it be an action
	class Throw: public Action {
	public:
		explicit Throw(lexer::SourcePosition position): Action(std::move(position)) {}

		void dprint(std::ostream& out) const final;
		virtual ~Throw() = default;
	};

	class Const: public Stmt {
		tpc::Identifier name;
		// @TODO: type should be expr in the future
		tpc::Identifier type;

	public:
		STMT_CHILD_CONSTRUCTOR(Const);
		static ParserRef<Const> parse(RiftParserState& state);

		base::StrId getName() const { return name.value; }

		~Const() final = default;
		void dprint(std::ostream& out) const final;
	};

	// @TODO: should assert be an action
	class Decl: public Stmt {
	public:
		Decl(StmtKind kind, lexer::SourcePosition position): Stmt(kind, std::move(position)) {}

		static ParserRef<Decl> parse(RiftParserState& state);
		bool                   trailingSemicolon() override;
	};

#define DECL_CHILD_CONSTRUCTOR(class_name) \
	class_name(lexer::SourcePosition position): Decl(StmtKind::class_name, std::move(position)) {}

	class CodeDecl: public Decl {
	public:
		DECL_CHILD_CONSTRUCTOR(CodeDecl);
	};

	class TopLevel: public Decl {
		std::vector<tpc::ParserRef<Stmt>> statements;

	public:
		DECL_CHILD_CONSTRUCTOR(TopLevel);
		static ParserRef<TopLevel> parse(RiftParserState& state);

		~TopLevel() final = default;
		void dprint(std::ostream& out) const final;

		const auto& getStatements() const { return statements; }
	};

	class Block: public CodeDecl {
		tpc::OptionalIdentifier optional_name;
		ParserRef<CodeBlock>    code_block = nullptr;

	public:
		explicit Block(lexer::SourcePosition position): CodeDecl(std::move(position)) {}

		static ParserRef<Block> parse(RiftParserState& state);
		virtual ~Block() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class Namespace: public Decl {
		tpc::Identifier      name;
		ParserRef<CodeBlock> body = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Namespace);

		base::StrId getName() const { return name.value; }

		ParserCBorrowRef<CodeBlock> getBody() const { return body.borrow(); }

		static ParserRef<Namespace> parse(RiftParserState& state);
		virtual ~Namespace() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class Struct: public Decl {
		tpc::Identifier              name;
		std::vector<ParserRef<Expr>> bases;
		ParserRef<CodeBlock>         body = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Struct);

		base::StrId getName() const { return name.value; }

		static ParserRef<Struct> parse(RiftParserState& state);
		virtual ~Struct() = default;
		virtual void dprint(std::ostream& out) const final;
	};

	class Fun: public Decl {
		tpc::Identifier            name;
		ParserRef<ParamList>       params = nullptr;
		ParserRef<RetList>         rets   = nullptr;
		ParserRef<CodeBlockOrStmt> body   = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Fun);

		base::StrId getName() const { return name.value; }

		static ParserRef<Fun> parse(RiftParserState& state);
		virtual void          dprint(std::ostream& out) const final;
		virtual ~Fun() = default;
	};

	class If: public CodeDecl {
		ParserRef<RoundGroupExpr>  condition = nullptr;
		tpc::OptionalIdentifier    optional_name;
		ParserRef<CodeBlockOrStmt> body = nullptr;

	public:
		explicit If(lexer::SourcePosition position): CodeDecl(std::move(position)) {}

		static ParserRef<If> parse(RiftParserState& state);
		virtual void         dprint(std::ostream& out) const final;
		virtual ~If() = default;
	};

	class While: public CodeDecl {
		ParserRef<RoundGroupExpr>  condition = nullptr;
		tpc::OptionalIdentifier    optional_name;
		ParserRef<CodeBlockOrStmt> body = nullptr;

	public:
		explicit While(lexer::SourcePosition position): CodeDecl(std::move(position)) {}

		static ParserRef<While> parse(RiftParserState& state);
		virtual void            dprint(std::ostream& out) const final;
		virtual ~While() = default;
	};

	class RiftTestingStmt: public Stmt {
	public:
		RiftTestingStmt(StmtKind kind, lexer::SourcePosition position):
			  Stmt(kind, std::move(position)) {}
	};

#define RIFT_TEST_CHILD_CONSTRUCTOR(class_name) \
	class_name(lexer::SourcePosition position): \
		  RiftTestingStmt(StmtKind::class_name, std::move(position)) {}

	class EagerLookup: public RiftTestingStmt {
		DottedName names;

	public:
		RIFT_TEST_CHILD_CONSTRUCTOR(EagerLookup);
		static ParserRef<EagerLookup> parse(RiftParserState& state);
		virtual void                  dprint(std::ostream& out) const final;
		virtual ~EagerLookup() = default;
	};

}
