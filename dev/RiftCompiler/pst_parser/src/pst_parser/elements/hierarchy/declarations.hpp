#pragma once

#include "statements.hpp"

namespace pst {
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

	class Class final: public Decl {
	private:
		tpc::Identifier           name;
		ParserRef<Expr>           base       = nullptr;
		ParserRef<ImplementsList> implements = nullptr;
		ParserRef<ClassBlock>     body       = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Class);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		ParserCBorrowRef<ClassBlock> getBody() const {
			return body.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getBase() const {
			return base.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<ImplementsList> getImplements() const {
			return implements.borrow();
		}

		static ParserRef<Class> parse(RiftParserState& state);
		~Class() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class";
		}

		[[nodiscard]]
		bool isStatementAggregate() const override {
			return true;
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
		~Variable() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return is_const ? "Let" : "Var";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	using ParamList = List<
		FunParam,
		false,
		lexer::Token::BracketType::Round,
		detail::Conditions::isComma,
		detail::Conditions::isSentinel,
		detail::NameGetters::parameterList>;

	class Fun final: public Decl {
		tpc::Identifier                 name;
		ParserRef<ParamList>            params = nullptr;
		base::Optional<ParserRef<Expr>> ret;
		ParserRef<CodeBlockOrStmt>      body = nullptr;

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
		auto getRet() const {
			return ret.map([](const auto& v) { return v.borrow(); });
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

	class If final: public CodeDecl {
		ParserRef<RoundGroupExpr>  condition = nullptr;
		tpc::OptionalIdentifier    optional_name;
		ParserRef<CodeBlockOrStmt> body      = nullptr;
		ParserRef<CodeBlockOrStmt> else_body = nullptr;

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

	class For final: public CodeDecl {
		tpc::OptionalIdentifier    optional_name;
		tpc::Identifier            iterator;
		ParserRef<Expr>            type     = nullptr;
		ParserRef<Expr>            iterable = nullptr;
		ParserRef<CodeBlockOrStmt> body     = nullptr;

	public:
		explicit For(const dia::SourcePosition& position): CodeDecl(position) {}

		static ParserRef<For> parse(RiftParserState& state);
		void                  dprint(std::ostream& out) const final;
		~For() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "For";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

}
