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

#include "meta.hpp"
#include "lists.hpp"

namespace pst {

	class FunParam final: public NotStmt {
		tpc::Identifier                        name;
		ParserRef<ExprElement>                 type;
		base::Optional<ParserRef<ExprElement>> initial;

	public:
		explicit FunParam(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<FunParam> parse(LangParserState& state);
		~FunParam() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function Parameter";
		}

		[[nodiscard]]
		ParserCBorrowRef<ExprElement> getType() const {
			return type.borrow();
		}
	};

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

		static ParserRef<DottedName> parse(LangParserState& state);

		[[nodiscard]]
		std::vector<base::StrID> getNames() const;
		[[nodiscard]]
		bool getStar() const;

		void dprint(std::ostream& out) const final;
		~DottedName() final = default;
	};

	class CodeBlock final: public NotStmt {
		std::vector<ParserRef<Stmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, Stmt)

		explicit CodeBlock(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<CodeBlock> parse(LangParserState& state);
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

	class ClassBlock final: public NotStmt {
		std::vector<ParserRef<ClassStmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, ClassStmt)

		explicit ClassBlock(const dia::SourcePosition& pos): NotStmt(pos) {};
		static ParserRef<ClassBlock> parse(LangParserState& state, const ClassContext& ctx);

		~ClassBlock() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Block";
		}

		// [[nodiscard]]
		// bool isStatementAggregate() const final {
		// 	return true;
		// }
	};

	class CodeBlockOrStmt final: public NotStmt {
		std::variant<ParserRef<Stmt>, ParserRef<CodeBlock>> content;

	public:
		explicit CodeBlockOrStmt(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<CodeBlockOrStmt> parse(LangParserState& state);
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
		ParserRef<ExprElement> expr = nullptr;

	public:
		explicit RoundGroupExpr(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<RoundGroupExpr> parse(LangParserState& state);
		~RoundGroupExpr() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Round Group Expression";
		}

		[[nodiscard]]
		ParserCBorrowRef<ExprElement> getExpr() const {
			return expr.borrow();
		}
	};

	/**
	 * @brief Common root for expression sub-elements
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
			  precedence(precedence) {}

	public:
		virtual void acceptVisitor(PstExprVisitor& visitor) const = 0;
	};

	class UniversalExpr: public NotStmt {
	public:
		static base::unique_ptr<ExprElement> parse(LangParserState& state);
		UniversalExpr() = delete;
	};

	class CommaExpr: public NotStmt {
	public:
		static base::unique_ptr<ExprElement> parse(LangParserState& state);
		CommaExpr() = delete;
	};
}
