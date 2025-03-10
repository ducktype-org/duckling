#pragma once

#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <lexer/token_common.hpp>

#include <lexer/token.hpp>
#include "meta.hpp"
#include "lists.hpp"           // IWYU pragma: keep
#include "not_statements.hpp"  // IWYU pragma: keep
#include <token_parser_core/common_elements.hpp>

#define CONDITION(name) static bool name(const LangParserState& state, i64 fwd = 0)

namespace pst {
	class CodeBlock;

	/**
	 * @brief For now these are some more general expr classification functions.
	 *
	 * @note This is temporary, it will be improved in the future.
	 */
	class ExprClassify {
	public:
		ExprClassify() = delete;

		CONDITION(isComparison) {
			static std::set<lang_def::NamedOperator> comparisons = {
				NamedOperator::Lesser, NamedOperator::LEqual, NamedOperator::Greater,
				NamedOperator::GEqual, NamedOperator::Equal,  NamedOperator::NotEqual,
			};
			return state[fwd].isOperator()
			    && comparisons.contains(state[fwd].asOperator().asNamed());
		}

		CONDITION(isAssignment) {
			return state[fwd].isOperator() && !isComparison(state, fwd)
			    && state[fwd].getStrValue().back() == '=';
		}

		CONDITION(exprStmtEnd) { return state[fwd].is(Special::Semicolon); }

		CONDITION(isOperator) { return state[fwd].isOperator(); }
	};

	namespace expr {
		/**
		 * @brief General parseUntil that allows to parse an expression element with a condition for
		 * expression end.
		 */
		template<std::derived_from<ExprElement> T, StateCondition until>
		MBox<ExprElement> parseUntil(LangParserState& state) {
			i64 length = 0;
			while (!state[length].is(lexer::Token::Type::Sentinel) && !until(state, length))
				length++;
			return T::parse(state, length);
		}

		/**
		 * @brief Common ancestor for prefix operator elements.
		 */
		class PrefixOperator: public ExprElement {
		protected:
			MBox<ExprElement> expr;
			Operator          op;

		public:
			explicit PrefixOperator(const dia::SourcePosition& pos, Operator op, i64 precedence):
				  ExprElement(pos, precedence),
				  op(op) {}

			~PrefixOperator() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			std::string elementType() const override {
				return "Prefix Operator";
			}

			[[nodiscard]]
			lexer::Operator getOperator() const;
			[[nodiscard]]
			MCRef<ExprElement> getExpr() const;
		};

		/**
		 * @brief Common ancestor for suffix operator elements.
		 */
		class SuffixOperator: public ExprElement {
		protected:
			Operator          op;
			MBox<ExprElement> expr;

		public:
			explicit SuffixOperator(const dia::SourcePosition& pos, Operator op, i64 precedence):
				  ExprElement(pos, precedence),
				  op(op) {}

			~SuffixOperator() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			std::string elementType() const override {
				return "Suffix Operator";
			}

			[[nodiscard]]
			lexer::Operator getOperator() const;
			[[nodiscard]]
			MCRef<ExprElement> getExpr() const;
		};

		/**
		 * @brief Common ancestor for binary operator elements.
		 */
		class BinaryOperator: public ExprElement {
		protected:
			MBox<ExprElement> left;
			Operator          op;
			MBox<ExprElement> right;

		public:
			explicit BinaryOperator(const dia::SourcePosition& pos, Operator op, i64 precedence):
				  ExprElement(pos, precedence),
				  op(op) {}

			~BinaryOperator() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			MCRef<ExprElement> getLeftOperand() const;
			[[nodiscard]]
			MCRef<ExprElement> getRightOperand() const;
			[[nodiscard]]
			lexer::Operator getOperator() const;

			[[nodiscard]]
			std::string elementType() const override {
				return "Binary Operator";
			}
		};

		/**
		 * @brief Element representing a number value in an expression
		 */
		class ExprValue final: public ExprElement {
			lexer::Value number;

		public:
			[[nodiscard]]
			lexer::Value getValue() const {
				return number;
			}

			explicit ExprValue(const dia::SourcePosition& position, lexer::Value value):
				  ExprElement(position, 0),
				  number(value) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~ExprValue() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			std::string elementType() const override {
				return "Value Expr";
			}
		};

		/**
		 * @brief This is a helper element for parsing literals and bracket subexpressions that
		 * decides which literal to parse.
		 */
		class Atom: public ExprElement {
		public:
			Atom() = delete;

			static MBox<ExprElement> parse(LangParserState& state, i64 length);
		};

		/**
		 * @brief Element representing template initialization in an expression. For example:
		 * `list:{i32}`.
		 *
		 * @note For now the inner expression is just a comma expression, this should probably have
		 * it's own parsing in the future
		 */
		class TemplateSpecifier final: public ExprElement {
			MBox<TemplateList> inner;

		public:
			TemplateSpecifier(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~TemplateSpecifier() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			std::string elementType() const override {
				return "Template Specifier Expression";
			}
		};

		/**
		 * @brief Element that represents an identifier literal in an expression
		 */
		class IdentifierLiteral final: public ExprElement {
			tpc::Identifier                   name;
			base::Optional<MBox<ExprElement>> template_specifier;

		public:
			IdentifierLiteral(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~IdentifierLiteral() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			const tpc::Identifier& getName() const {
				return name;
			}

			[[nodiscard]]
			std::string elementType() const override {
				return "Identifier Expression";
			}
		};

		/**
		 * @brief Element that represents an keyword literal in an expression
		 */
		class KeywordLiteral final: public ExprElement {
			Keyword                           keyword = Keyword::NotAKeyword;
			base::Optional<MBox<ExprElement>> template_specifier;

		public:
			KeywordLiteral(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~KeywordLiteral() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			Keyword getKeyword() const {
				return keyword;
			}

			[[nodiscard]]
			std::string elementType() const override {
				return "Keyword Expression";
			}
		};

		/**
		 * @brief This represents a single access expression of type `[expression operator like . or
		 * .?][name][optionally template specifier]`
		 */
		class Access final: public ExprElement {
			base::StrID                       type;  ///< either `.` or `.?`
			tpc::Identifier                   name;
			base::Optional<MBox<ExprElement>> template_specifier;

		public:
			Access(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~Access() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			std::string elementType() const override {
				return "Access Expression";
			}

			[[nodiscard]]
			base::StrID getType() const;
			[[nodiscard]]
			const tpc::Identifier& getName() const;

			[[nodiscard]]
			base::Optional<MCRef<ExprElement>> getTemplateSpecifier() const {
				return template_specifier.map([](const auto& t) { return t.ref(); });
			}
		};

		/**
		 * @brief Represents a single call or subscript expression
		 */
		class Call final: public ExprElement {
			lexer::Token::BracketType type
				= lexer::Token::BracketType::None;  ///< either Round or Square
			MBox<CallList> args;

		public:
			Call(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~Call() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]] lexer::Token::BracketType getType() const;

			[[nodiscard]] MCRef<CallList> getArgs() const;

			[[nodiscard]]
			std::string elementType() const override {
				return "Call Expression";
			}
		};

		/**
		 * @brief Combined chain of an atom followed by Accesses / Calls / Subscripts.
		 */
		class ChainExpr final: public ExprElement {
			using Lower = Atom;

			MBox<ExprElement>              atom;
			std::vector<MBox<ExprElement>> chain;

			/**
			 * @brief checks length before the start of the next link
			 */
			static i64 toNextLink(const LangParserState& state, i64 length);

		public:
			ChainExpr(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);
			void                     dprint(std::ostream& out) const final;
			void                     acceptExprVisitor(PstExprVisitor& visitor) const final;

			~ChainExpr() override = default;

			[[nodiscard]]
			std::string elementType() const override {
				return "Chain Expression";
			}

			[[nodiscard]]
			MCRef<ExprElement> getAtom() const;
			[[nodiscard]]
			const std::vector<MBox<ExprElement>>& getChain() const;
		};

		/**
		 * @brief Expression in round brackets
		 */
		class RoundExpr final: public ExprElement {
			MBox<ExprElement> expr;

		public:
			explicit RoundExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~RoundExpr() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			std::string elementType() const override {
				return "Round Group Expression";
			}

			[[nodiscard]]
			MCRef<ExprElement> getInner() const {
				return expr.ref();
			}
		};

		/**
		 * @brief Block expression
		 *
		 * A block that has value equal to the value returned from it.
		 */
		class BlockExpr final: public ExprElement {
			MBox<CodeBlock> block;

		public:
			explicit BlockExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~BlockExpr() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			MCRef<CodeBlock> getBlock() { return block.ref(); }

			[[nodiscard]]
			std::string elementType() const override {
				return "Block Expression";
			}
		};

		/**
		 * @brief General prefix operator
		 *
		 * Excludes `not`
		 */
		class GeneralPrefix final: public PrefixOperator {
			using Lower = ChainExpr;
			using Self  = GeneralPrefix;

		public:
			explicit GeneralPrefix(const dia::SourcePosition& pos, Operator op):
				  PrefixOperator(pos, op, 400) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~GeneralPrefix() override = default;
		};

		/**
		 * @brief General suffix operator
		 */
		class GeneralSuffix final: public SuffixOperator {
			using Lower = GeneralPrefix;
			using Self  = GeneralSuffix;

			static MBox<ExprElement> parseRecursive(LangParserState& state, i64 length, u64 iter);

		public:
			explicit GeneralSuffix(const dia::SourcePosition& pos, Operator op):
				  SuffixOperator(pos, op, 450) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~GeneralSuffix() override = default;
		};

		/**
		 * @brief General binary operator
		 *
		 * Excludes `and`, `or`
		 */
		class GeneralBinary final: public BinaryOperator {
			using Lower = GeneralSuffix;
			using Self  = GeneralBinary;

			/**
			 * @brief is a general binary operators
			 */
			CONDITION(isGenBinOp) {
				static const std::set<lang_def::NamedOperator> gen_bin_ops = {
					NamedOperator::Exponentiate, NamedOperator::Multiply, NamedOperator::Divide,
					NamedOperator::Remainder,    NamedOperator::Plus,     NamedOperator::Minus,
					NamedOperator::Pipe,
				};
				return gen_bin_ops.contains(state[fwd].asOperator().asNamed());
			}

			static i64 getOpPrec(Operator op) {
				static const std::unordered_map<lang_def::NamedOperator, i64> precedences = {
					{ NamedOperator::Pipe, 540 },      { NamedOperator::Exponentiate, 550 },
					{ NamedOperator::Multiply, 560 },  { NamedOperator::Divide, 560 },
					{ NamedOperator::Remainder, 560 }, { NamedOperator::Plus, 570 },
					{ NamedOperator::Minus, 570 },
				};
				if (not precedences.contains(op.asNamed()))
					throw base::NotYetImplemented(
						base::strConcat("Operator precedence for operator: ", op.value.strView())
					);
				return precedences.at(op.asNamed());
			}

			struct OperatorBuilder;

			using BuilderExpr = std::variant<i64, Box<OperatorBuilder>>;

			struct OperatorBuilder {
				BuilderExpr lhs;
				Operator    type;
				BuilderExpr rhs;
			};

		public:
			explicit GeneralBinary(const dia::SourcePosition& pos, Operator op):
				  BinaryOperator(pos, op, getOpPrec(op)) {}

			/**
			 * @brief
			 *
			 * @note Assumes an expression atom ends on either:
			 * 1. End of expression
			 * 2. A General binary operator
			 * 3. Literal that isn't following an access operator (`.`, in future also `.?`, maybe
			 * `::`)
			 *
			 */
			static i64 skipAtom(const LangParserState& state, i64 base, i64 length);

			static MBox<ExprElement>
				parseRecursive(LangParserState& state, const BuilderExpr& expr);

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~GeneralBinary() override = default;
		};

		/**
		 * @brief This class represents a chain of compared expressions for example: `0 < a + b <=
		 * c.size()`
		 *
		 * The chain is stored as a list of sub-expressions and a list of operators between them.
		 */
		class ComparisonChain final: public ExprElement {
			using Lower = GeneralBinary;

			std::vector<MBox<ExprElement>> sub_expr;
			std::vector<Operator>          operators;

			static i64 skipToOp(const LangParserState& state, i64 base, i64 length);

		public:
			ComparisonChain(const dia::SourcePosition& pos): ExprElement(pos, 600) {}

			[[nodiscard]]
			std::string elementType() const override {
				return "Comparison Chain";
			}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~ComparisonChain() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;
		};

		/**
		 * @brief Logical `not` operator.
		 */
		class LogicNot final: public PrefixOperator {
			using Lower = ComparisonChain;
			using Self  = LogicNot;

		public:
			explicit LogicNot(const dia::SourcePosition& position):
				  PrefixOperator(position, lang_def::keywordToStr(Keyword::Not), 730) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~LogicNot() override = default;
		};

		/**
		 * @brief Logical `and` operator.
		 */
		class LogicAnd final: public BinaryOperator {
			using Lower = LogicNot;
			using Self  = LogicAnd;

		public:
			explicit LogicAnd(const dia::SourcePosition& position):
				  BinaryOperator(position, lang_def::keywordToStr(lang_def::Keyword::And), 730) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~LogicAnd() override = default;
		};

		/**
		 * @brief Logical `or` operator.
		 */
		class LogicOr final: public BinaryOperator {
			using Lower = LogicAnd;
			using Self  = LogicOr;

		public:
			explicit LogicOr(const dia::SourcePosition& position):
				  BinaryOperator(position, lang_def::keywordToStr(lang_def::Keyword::Or), 760) {}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~LogicOr() override = default;
		};

		/**
		 * @brief Ternary expression(`if condition then if_true else if_else`).
		 *
		 * @note For now the parsing of ternary is pretty limited with only one such expression
		 * without any parenthesis. This is a limited but safe option.
		 *
		 * @note This should be the default starting level for an expression when comma expression
		 * would cause parsing problems.
		 */
		class Ternary final: public ExprElement {
			using Lower = LogicOr;

			MBox<ExprElement> condition;
			MBox<ExprElement> if_true;
			MBox<ExprElement> if_false;

		public:
			explicit Ternary(const dia::SourcePosition& position): ExprElement(position, 800) {}

			[[nodiscard]]
			std::string elementType() const override {
				return "Ternary Expr";
			}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~Ternary() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;
		};

		/**
		 * @brief Comma separated expression.
		 *
		 * @note This is the second possible entry point for expression parsing when comma
		 * expression doesn't cause problems with other parsing.
		 */
		class Comma final: public ExprElement {
			using Lower = Ternary;

			std::vector<MBox<ExprElement>> expressions;

		public:
			explicit Comma(const dia::SourcePosition& position): ExprElement(position, 900) {}

			[[nodiscard]]
			std::string elementType() const override {
				return "Comma Expr";
			}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~Comma() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;

			[[nodiscard]]
			const std::vector<MBox<ExprElement>>& getExpressions() const;
		};

		/**
		 * @brief This is an assignment expression.
		 *
		 * @note An assignment expression is supposed to appear only once in a stmt expression.
		 */
		class Assignment final: public ExprElement {
			using Lower = Comma;

			MBox<ExprElement> variables;
			base::StrID       type;
			MBox<ExprElement> value;

		public:
			explicit Assignment(const dia::SourcePosition& position):
				  ExprElement(position, 1'000) {}

			[[nodiscard]]
			std::string elementType() const override {
				return "Assignment Expr";
			}

			[[nodiscard]]
			MCRef<ExprElement> getVariables() const {
				return variables.ref();
			}

			[[nodiscard]]
			base::StrID getAssignmentType() const {
				return type;
			}

			[[nodiscard]]
			MCRef<ExprElement> getValue() const {
				return value.ref();
			}

			static MBox<ExprElement> parse(LangParserState& state, i64 length);

			~Assignment() override = default;
			void dprint(std::ostream& out) const final;
			void acceptExprVisitor(PstExprVisitor& visitor) const final;
		};
	}
}
