#pragma once

#include "../../rift_parser_state.hpp"
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

namespace pst {
	class Expr;

	/**
	 * @brief General Element representing a list of Elements.
	 *
	 * @tparam ListElements - Kept Elements, has to have precise length parse like Expr
	 * @tparam NON_EMPTY - Should empty list be an error.
	 * @tparam BRACKETS - expected brackets or None if not expected
	 * @tparam isSeparator - Separator should always be skip-able with one skip.
	 * @tparam isEnding - Check for successful ending.
	 * @tparam getName - List name getter for errors.
	 * @tparam Container - Vector-like container of SubElements with emplace_back. Possibly with
	 * other condition because of iteration.
	 */
	template<
		class ListElements,
		bool                      NON_EMPTY,
		lexer::Token::BracketType BRACKETS,
		StateCondition            isSeparator,
		StateCondition            isEnding,
		GetName                   getName,
		class Container = std::vector<ParserRef<ListElements>>>
	class List final: public NotStmt {
		Container elements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(elements, ListElements)

		[[nodiscard]]
		usize size() const {
			return elements.size();
		}

		explicit List(const dia::SourcePosition& position): NotStmt(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return getName() + " list";
		}

		static ParserRef<List> parse(RiftParserState& state);

		void dprint(std::ostream& out) const final {
			out << "[";
			for (auto& x: elements) {
				tpc::nullAwareDprint(x, out);
				out << ",";
			}
			out << "]";
		}

		~List() final = default;
	};

	class FunParam final: public NotStmt {
		tpc::Identifier                 name;
		ParserRef<Expr>                 type;
		base::Optional<ParserRef<Expr>> initial;

	public:
		explicit FunParam(const dia::SourcePosition& position): NotStmt(position) {}

		static ParserRef<FunParam> parse(RiftParserState& state);
		~FunParam() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function Parameter";
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getType() const {
			return type.borrow();
		}
	};

	using RetList = List<
		Expr,
		true,
		lexer::Token::BracketType::None,
		detail::Conditions::isComma,
		detail::Conditions::isAssign,
		detail::NameGetters::returnList>;

	using ImplementsList = List<
		Expr,
		true,
		lexer::Token::BracketType::None,
		detail::Conditions::isComma,
		detail::Conditions::isBlockGroup,
		detail::NameGetters::inheritanceList>;

	using AtrArgList = List<
		Expr,
		false,
		lexer::Token::BracketType::Round,
		detail::Conditions::isComma,
		detail::Conditions::isSentinel,
		detail::NameGetters::attributeArgList>;

	using InitList = List<
		Expr,
		false,
		lexer::Token::BracketType::None,
		detail::Conditions::isComma,
		detail::Conditions::isAssign,
		detail::NameGetters::classInitList>;

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

	class Attribute final: public NotStmt {
		ParserRef<DottedName> name;
		ParserRef<AtrArgList> args = nullptr;

	public:
		explicit Attribute(dia::SourcePosition& pos): NotStmt(pos) {}

		static ParserRef<Attribute> parse(RiftParserState& state);
		~Attribute() final = default;

		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Attribute";
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

	class ClassBlock final: public NotStmt {
		std::vector<ParserRef<ClassStmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, ClassStmt)

		explicit ClassBlock(const dia::SourcePosition& pos): NotStmt(pos){};
		static ParserRef<ClassBlock> parse(RiftParserState& state, const ClassContext& ctx);

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
	class Expr final: public NotStmt {
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

		explicit Expr(const dia::SourcePosition& position): NotStmt(position) {}

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
	};

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
		class ListElements,
		bool                      NON_EMPTY,
		lexer::Token::BracketType BRACKETS,
		StateCondition            isSeparator,
		StateCondition            isEnding,
		GetName                   getName,
		class Container>
	auto List<ListElements, NON_EMPTY, BRACKETS, isSeparator, isEnding, getName, Container>::parse(
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

				ParserRef<ListElements> ref;
				// @TODO: This is a "temporary" fix.
				// Hopefully we can handle this with a uniform `parse` function for all
				// elements, maybe by polymorphism.
				if constexpr (std::derived_from<ListElements, Expr>) {
					state.parse(out).template with<ListElements>(
						&ref, ListElements::parse, (usize) expr_length, true, false
					);
				} else {
					state.parse(out).template with<ListElements>(&ref, ListElements::parse);
				}
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
