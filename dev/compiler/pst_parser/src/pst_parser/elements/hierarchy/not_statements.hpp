#pragma once

#include "../../lang_parser_state.hpp"
#include "../elements_common.hpp"
#include "expr_holders.hpp"
#include "meta.hpp"

#include <diagnostic/source_position.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/token_stream.hpp>

#include <base/string_id.hpp>

namespace pst {

	/**
	 * @brief Declaration of a single function argument.
	 */
	class FunParam final: public NotStmt {
		tpc::Identifier                                     name;
		AccessInternal<UniversalExprHolder>                 type;
		base::Optional<AccessInternal<UniversalExprHolder>> initial;

	public:
		explicit FunParam(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::FunParam;
		}

		static MBox<FunParam> parse(LangParserState& state);
		~FunParam() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function Parameter";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getType() const {
			return type.give();
		}

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<UniversalExprHolder>> getValue() const;

		void acceptVisitor(PstVisitor& visitor) const final;
	};

	/**
	 * @brief Simple dotted name that is Identifiers separated by dots potentially ended by `.*`
	 *
	 * used for imports
	 */
	class DottedName final: public NotStmt {
		std::vector<tpc::Identifier> names;
		bool                         star = false;

	public:
		explicit DottedName(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::DottedName;
		}

		[[nodiscard]]
		const std::vector<tpc::Identifier>& getNames() const {
			return names;
		}

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

		static MBox<DottedName> parse(LangParserState& state);

		[[nodiscard]]
		bool getStar() const;

		void dprint(std::ostream& out) const final;
		~DottedName() final = default;
	};

	/**
	 * @brief Code Block that contains statements.
	 */
	class CodeBlock final: public NotStmt {
		std::vector<AccessInternal<Stmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, Stmt)

		explicit CodeBlock(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlock;
		}

		static MBox<CodeBlock> parse(LangParserState& state);
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

	/**
	 * @brief Class Block that contains Class statements.
	 */
	class ClassBlock final: public NotStmt {
		std::vector<AccessInternal<ClassStmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, ClassStmt)

		explicit ClassBlock(const dia::SourcePosition& pos): NotStmt(pos) {
			this->element_kind = ElementKind::ClassBlock;
		}

		static MBox<ClassBlock> parse(LangParserState& state, const ClassContext& ctx);

		~ClassBlock() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}
	};

	/**
	 * @brief Code Block or Statement.
	 */
	class CodeBlockOrStmt final: public NotStmt {
		std::variant<AccessInternal<Stmt>, AccessInternal<CodeBlock>> content;

	public:
		explicit CodeBlockOrStmt(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlockOrStmt;
		}

		static MBox<CodeBlockOrStmt> parse(LangParserState& state);
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

	/**
	 * @brief Expression surrounded by parenthesis.
	 */
	class RoundGroupExpr final: public NotStmt {
		AccessInternal<CommaExprHolder> expr;

	public:
		explicit RoundGroupExpr(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::RoundGroupExpr;
		}

		static MBox<RoundGroupExpr> parse(LangParserState& state);
		~RoundGroupExpr() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Round Group Expression";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getExpr() const {
			return expr.give();
		}
	};

	/**
	 * @brief Common root for expression sub-elements.
	 */
	class ExprElement: public NotStmt {
		const i64 precedence;

	protected:
		/**
		 * @brief Skips tokens, used to preserve position in case of error.
		 */
		static void fastForward(LangParserState& state, i64 length);

		/**
		 * @brief Sanity check of length.
		 */
		static bool checkLength(LangParserState& state, i64 length);

		explicit ExprElement(const dia::SourcePosition& position, i64 precedence):
			  NotStmt(position),
			  precedence(precedence) {
			this->element_kind = ElementKind::ExprElement;
		}

	public:
		virtual void acceptExprVisitor(expr::PstExprVisitor& visitor) const = 0;
	};

}
