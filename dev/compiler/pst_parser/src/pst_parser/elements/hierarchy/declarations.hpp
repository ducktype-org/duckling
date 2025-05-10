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
		std::vector<AccessInternal<Stmt>> statements;

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
		auto getStatements() const {
			using namespace std::views;
			static auto give_one = [](auto& acc) -> AccessLocked<Stmt> { return acc.give(); };
			return std::ranges::ref_view(statements) | transform(give_one);
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
		tpc::OptionalIdentifier   optional_name;
		AccessInternal<CodeBlock> code_block;

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
		tpc::Identifier           name;
		AccessInternal<CodeBlock> body;

	public:
		DECL_CHILD_CONSTRUCTOR(Namespace, ElementKind::Namespace);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<CodeBlock> getBody() const {
			return body.give();
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
		tpc::Identifier                name;
		AccessInternal<ExprElement>    base;
		AccessInternal<ImplementsList> implements;
		AccessInternal<ClassBlock>     body;

	public:
		DECL_CHILD_CONSTRUCTOR(Class, ElementKind::Class);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ClassBlock> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getBase() const {
			return base.give();
		}

		[[nodiscard]]
		AccessLocked<ImplementsList> getImplements() const {
			return implements.give();
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
	 * @brief Const compile time variable declaration.
	 */
	class Const final: public Decl {
		tpc::Identifier                                 name;
		base::Optional<AccessInternal<CommaExprHolder>> type;
		base::Optional<AccessInternal<CommaExprHolder>> value;

		template<typename T, lang_def::Keyword key>
		friend MBox<T> parseVariableTemplate(pst::LangParserState& state);

	public:
		DECL_CHILD_CONSTRUCTOR(Const, ElementKind::Const);
		static MBox<Const> parse(LangParserState& state);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		bool trailingSemicolon() override;

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getType() const {
			if (type.has_value()) return { type.value().give() };
			return {};
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getValue() const {
			if (value.has_value()) return { value.value().give() };
			return {};
		}

		~Const() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Const";
		}
	};

	/**
	 * @brief Variable declaration
	 */
	class Variable final: public Decl {
		tpc::Identifier                                 name;
		base::Optional<AccessInternal<CommaExprHolder>> type;
		base::Optional<AccessInternal<CommaExprHolder>> value;
		bool                                            is_const = true;

		template<typename T, lang_def::Keyword key>
		friend MBox<T> parseVariableTemplate(pst::LangParserState& state);

	public:
		DECL_CHILD_CONSTRUCTOR(Variable, ElementKind::Variable);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		bool trailingSemicolon() override;

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getType() const {
			if (type.has_value()) return { type.value().give() };
			return {};
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getValue() const {
			if (value.has_value()) return { value.value().give() };
			return {};
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
		tpc::Identifier                                 name;
		AccessInternal<ParamList>                       params;
		base::Optional<AccessInternal<CommaExprHolder>> ret;
		AccessInternal<CodeBlockOrStmt>                 body;

	public:
		DECL_CHILD_CONSTRUCTOR(Fun, ElementKind::Fun);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		[[nodiscard]]
		/**
		 * @note Optional of MCRef here is intentional
		 */
		base::Optional<AccessLocked<ExprHolder>> getRet() const {
			return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
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
		AccessInternal<RoundGroupExpr>  condition;
		tpc::OptionalIdentifier         optional_name;
		AccessInternal<CodeBlockOrStmt> body;
		AccessInternal<CodeBlockOrStmt> else_body;

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
		AccessLocked<ExprHolder> getCondition() const {
			return condition.internal()->getExpr();
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief While declaration
	 */
	class While final: public CodeDecl {
		AccessInternal<RoundGroupExpr>  condition;
		tpc::OptionalIdentifier         optional_name;
		AccessInternal<CodeBlockOrStmt> body;

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
		tpc::OptionalIdentifier           optional_name;
		tpc::Identifier                   iterator;
		AccessInternal<ForTypeExprHolder> type;
		AccessInternal<CommaExprHolder>   iterable;
		AccessInternal<CodeBlockOrStmt>   body;

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
