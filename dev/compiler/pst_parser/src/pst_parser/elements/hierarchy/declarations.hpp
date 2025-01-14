#pragma once

#include "statements.hpp"

namespace pst {
#define DECL_CHILD_CONSTRUCTOR(class_name, element_type_)                                   \
	class_name(const dia::SourcePosition& position): Decl(StmtKind::class_name, position) { \
		this->element_kind = element_type_;                                                 \
	}

#define DECL_CHILD_CONSTRUCTOR_NO_KIND(class_name) \
	class_name(const dia::SourcePosition& position): Decl(StmtKind::class_name, position) {}

	/**
	 * @brief Common ancestor element for code declarations.
	 *
	 * Code declarations are statements that can generally create new symbols like function
	 * declarations, variable declarations or language construct with names like fors, blocks and
	 * whiles
	 */
	class CodeDecl: public Decl {
	public:
		DECL_CHILD_CONSTRUCTOR_NO_KIND(CodeDecl);

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Declaration";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return false;
		}
	};

	/**
	 * @brief Top-level element that is the root of the pst of a single file.
	 */
	class TopLevel final: public Decl {
		std::vector<MBox<Stmt>> statements;

	public:
		DECL_CHILD_CONSTRUCTOR(TopLevel, ElementKind::TopLevel);

		static MBox<TopLevel> parse(LangParserState& state);

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

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Block declaration
	 */
	class Block final: public CodeDecl {
		tpc::OptionalIdentifier optional_name;
		MBox<CodeBlock>         code_block = nullptr;

	public:
		explicit Block(const dia::SourcePosition& position): CodeDecl(position) {
			element_kind = ElementKind::Block;
		}

		static MBox<Block> parse(LangParserState& state);
		~Block() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Block";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Namespace declaration
	 */
	class Namespace final: public Decl {
		tpc::Identifier name;
		MBox<CodeBlock> body = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Namespace, ElementKind::Namespace);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		MCRef<CodeBlock> getBody() const {
			return body.ref();
		}

		static MBox<Namespace> parse(LangParserState& state);
		~Namespace() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Namespace";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Class declaration
	 */
	class Class final: public Decl {
	private:
		tpc::Identifier      name;
		MBox<ExprElement>    base       = nullptr;
		MBox<ImplementsList> implements = nullptr;
		MBox<ClassBlock>     body       = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Class, ElementKind::Class);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		MCRef<ClassBlock> getBody() const {
			return body.ref();
		}

		[[nodiscard]]
		MCRef<ExprElement> getBase() const {
			return base.ref();
		}

		[[nodiscard]]
		MCRef<ImplementsList> getImplements() const {
			return implements.ref();
		}

		static MBox<Class> parse(LangParserState& state);
		~Class() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class";
		}

		// [[nodiscard]]
		// bool isStatementAggregate() const override {
		// 	return true;
		// }

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Variable declaration
	 */
	class Variable final: public Decl {
		tpc::Identifier   name;
		MBox<ExprElement> type     = nullptr;
		MBox<ExprElement> value    = nullptr;
		bool              is_const = true;

	public:
		DECL_CHILD_CONSTRUCTOR(Variable, ElementKind::Variable);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		bool trailingSemicolon() override;

		[[nodiscard]]
		MCRef<ExprElement> getType() const {
			return type.ref();
		}

		[[nodiscard]]
		MCRef<ExprElement> getValue() const {
			return value.ref();
		}

		[[nodiscard]]
		bool isConst() const {
			return is_const;
		}

		static MBox<Variable> parse(LangParserState& state);
		~Variable() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return is_const ? "Let" : "Var";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Function declaration
	 */
	class Fun final: public Decl {
		tpc::Identifier                   name;
		MBox<ParamList>                   params = nullptr;
		base::Optional<MBox<ExprElement>> ret;
		MBox<CodeBlockOrStmt>             body = nullptr;

	public:
		DECL_CHILD_CONSTRUCTOR(Fun, ElementKind::Fun);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		MCRef<ParamList> getParams() const {
			return params.ref();
		}

		[[nodiscard]]
		/**
		 * @note Optional of MCRef here is intentional
		 */
		base::Optional<MCRef<ExprElement>> getRet() const {
			return ret.map([](const auto& v) { return v.ref(); });
		}

		[[nodiscard]]
		MCRef<CodeBlockOrStmt> getBody() const {
			return body.ref();
		}

		static MBox<Fun> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~Fun() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief If declaration
	 */
	class If final: public CodeDecl {
		MBox<RoundGroupExpr>    condition = nullptr;
		tpc::OptionalIdentifier optional_name;
		MBox<CodeBlockOrStmt>   body      = nullptr;
		MBox<CodeBlockOrStmt>   else_body = nullptr;

	public:
		explicit If(const dia::SourcePosition& position): CodeDecl(position) {
			element_kind = ElementKind::If;
		}

		static MBox<If> parse(LangParserState& state);
		void            dprint(std::ostream& out) const final;
		~If() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "If";
		}

		[[nodiscard]]
		MCRef<ExprElement> getCondition() const {
			return condition->getExpr();
		}

		[[nodiscard]]
		MCRef<CodeBlockOrStmt> getBody() const {
			return body.ref();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief While declaration
	 */
	class While final: public CodeDecl {
		MBox<RoundGroupExpr>    condition = nullptr;
		tpc::OptionalIdentifier optional_name;
		MBox<CodeBlockOrStmt>   body = nullptr;

	public:
		explicit While(const dia::SourcePosition& position): CodeDecl(position) {
			element_kind = ElementKind::While;
		}

		static MBox<While> parse(LangParserState& state);
		void               dprint(std::ostream& out) const final;
		~While() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "While";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief For declaration
	 */
	class For final: public CodeDecl {
		tpc::OptionalIdentifier optional_name;
		tpc::Identifier         iterator;
		MBox<ExprElement>       type     = nullptr;
		MBox<ExprElement>       iterable = nullptr;
		MBox<CodeBlockOrStmt>   body     = nullptr;

	public:
		explicit For(const dia::SourcePosition& position): CodeDecl(position) {}

		static MBox<For> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~For() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "For";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

}
