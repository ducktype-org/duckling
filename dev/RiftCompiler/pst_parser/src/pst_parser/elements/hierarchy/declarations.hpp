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
		void semPrint(std::ostream& out) const final;

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
		void semPrint(std::ostream& out) const final;

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
		void semPrint(std::ostream& out) const final;

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
		void semPrint(std::ostream& out) const final;

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
		void                  semPrint(std::ostream& out) const final;
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
		void                       semPrint(std::ostream& out) const override;
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
		void                 semPrint(std::ostream& out) const final;
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
		void                    semPrint(std::ostream& out) const final;
		~While() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "While";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};
}
