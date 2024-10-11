#pragma once

#include "meta.hpp"
#include "lists.hpp"           // IWYU pragma: keep
#include "not_statements.hpp"  // IWYU pragma: keep

#define CONDITION(name) static bool name(const RiftParserState& state, i64 fwd = 0)

namespace pst {
	class CodeBlock;

	/**
	 * @brief Common root for expression sub-elements
	 */
	class ExprElement: public NotStmt {
		const i64 precedence;

	protected:
		/**
		 * @brief Skips tokens, used to preserve position in case of error.
		 */
		static void fastForward(RiftParserState& state, i64 length);

		/**
		 * @brief Sanity check of length.
		 */
		static bool checkLength(RiftParserState& state, i64 length);

		void dprint(std::ostream& out) const override { out << "<SUBEXPR UNIMPLEMENTED>"; }

		explicit ExprElement(const dia::SourcePosition& position, i64 precedence):
			  NotStmt(position),
			  precedence(precedence) {}
	};

	/**
	 * @brief This should be generalized or made into separate parts in logical places.
	 */
	class ExprClassify {
	public:
		ExprClassify() = delete;

		CONDITION(isComparison) {
			static std::set<rift_def::Operator> comparisons = {
				Operator::Lesser, Operator::LEqual, Operator::Greater,
				Operator::GEqual, Operator::Equal,  Operator::NotEqual,
			};
			return state[fwd].isOperator() && comparisons.contains(state[fwd].asOperator());
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
		 * @brief General parseUntil
		 */
		template<std::derived_from<ExprElement> T, StateCondition until>
		ParserRef<ExprElement> parseUntil(RiftParserState& state) {
			i64 length = 0;
			while (!state[length].is(lexer::Token::Type::Sentinel) && !until(state, length))
				length++;
			return T::parse(state, length);
		}

		class PrefixOperator: public ExprElement {
		protected:
			ParserRef<ExprElement> expr;
			base::StrId            type;

		public:
			explicit PrefixOperator(
				const dia::SourcePosition& pos, base::StrId type, i64 precedence
			):
				  ExprElement(pos, precedence),
				  type(type) {}

			std::string elementType() const override { return type.str() + " Prefix Operator"; }
		};

		class SuffixOperator: public ExprElement {
		protected:
			base::StrId            type;
			ParserRef<ExprElement> expr;

		public:
			explicit SuffixOperator(
				const dia::SourcePosition& pos, base::StrId type, i64 precedence
			):
				  ExprElement(pos, precedence),
				  type(type) {}

			std::string elementType() const override { return type.str() + " Suffix Operator"; }
		};

		class BinaryOperator: public ExprElement {
		protected:
			ParserRef<ExprElement> left;
			base::StrId            type;
			ParserRef<ExprElement> right;

		public:
			explicit BinaryOperator(
				const dia::SourcePosition& pos, base::StrId type, i64 precedence
			):
				  ExprElement(pos, precedence),
				  type(type) {}

			std::string elementType() const override { return type.str() + " Infix Operator"; }
		};

		class Value: public ExprElement {
			base::StrId number;

		public:
			explicit Value(const dia::SourcePosition& position): ExprElement(position, 0) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Value Expr"; }
		};

		class Literal final: public ExprElement {
		public:
			Literal() = delete;

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class TemplateSpecifier final: public ExprElement {
			ParserRef<ExprElement> inner;

		public:
			TemplateSpecifier(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Template Specifier Expression"; }
		};

		class IdentifierLiteral: public ExprElement {
			tpc::Identifier                        name;
			base::Optional<ParserRef<ExprElement>> template_specifier;

		public:
			IdentifierLiteral(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Identifier Expression"; }
		};

		class Access: public ExprElement {
			base::StrId                            type;  ///< either `.` or `.?`
			tpc::Identifier                        name;
			base::Optional<ParserRef<ExprElement>> template_specifier;

		public:
			Access(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Access Expression"; }
		};

		/**
		 * @brief Call or Subscript
		 *
		 * @note Currently an empty call/subscript results in an error.
		 */
		class Call: public ExprElement {
			lexer::Token::BracketType type;  ///< either Round or Square
			ParserRef<ExprElement>    args;

		public:
			Call(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Call Expression"; }
		};

		/**
		 * @brief Combined Access / Call / Subscirpt.
		 */
		class ChainExpr: public ExprElement {
			using Lower = Literal;

			ParserRef<ExprElement>              literal;
			std::vector<ParserRef<ExprElement>> chain;

			/**
			 * @brief checks length before the start of the next link
			 */
			static u64 toNextLink(const RiftParserState& state, u64 length);

		public:
			ChainExpr(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Chain Expression"; }
		};

		class RoundExpr: public ExprElement {
			ParserRef<ExprElement> expr;

		public:
			explicit RoundExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Round Group Expression"; }
		};

		class BlockExpr: public ExprElement {
			ParserRef<CodeBlock> block;

		public:
			explicit BlockExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Block Expression"; }
		};

		class GeneralPrefix: public PrefixOperator {
			using Lower = ChainExpr;
			using Self  = GeneralPrefix;

		public:
			explicit GeneralPrefix(const dia::SourcePosition& pos, base::StrId op):
				  PrefixOperator(pos, op, 400) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class GeneralSuffix: public SuffixOperator {
			using Lower = GeneralPrefix;
			using Self  = GeneralSuffix;

			static ParserRef<ExprElement>
				parseRecursive(RiftParserState& state, u64 length, u64 iter);

		public:
			explicit GeneralSuffix(const dia::SourcePosition& pos, base::StrId op):
				  SuffixOperator(pos, op, 450) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class GeneralBinary: public BinaryOperator {
			using Lower = GeneralSuffix;
			using Self  = GeneralBinary;

			/**
			 * @brief is a general binary operators
			 */
			CONDITION(isGenBinOp) {
				static const std::set<rift_def::Operator> gen_bin_ops = {
					Operator::Multiply,
					Operator::Divide,
					Operator::Plus,
					Operator::Minus,
				};
				return gen_bin_ops.contains(state[fwd].asOperator());
			}

			static i64 getOpPrec(Operator op) {
				static const std::unordered_map<rift_def::Operator, i64> precedences = {
					{ Operator::Multiply, 510 },
					{ Operator::Divide, 510 },
					{ Operator::Plus, 520 },
					{ Operator::Minus, 520 },
				};
				return precedences.at(op);
			}

			struct OperatorBuilder;

			using BuilderExpr = std::variant<u64, base::unique_ptr<OperatorBuilder>>;

			struct OperatorBuilder {
				BuilderExpr lhs;
				Operator    type;
				BuilderExpr rhs;
			};

		public:
			explicit GeneralBinary(const dia::SourcePosition& pos, rift_def::Operator op):
				  BinaryOperator(pos, rift_def::operatorToStr(op), getOpPrec(op)) {}

			/**
			 * @brief
			 *
			 * @note Assumes an expression "literal" ends on either:
			 * 1. End of expression
			 * 2. A General binary operator
			 * 3. Literal that isn't following an access operator (`.`, in future also `.?`, maybe
			 * `::`)
			 *
			 */
			static u64 skipLiteral(const RiftParserState& state, u64 base, u64 length);

			static ParserRef<ExprElement>
				parseRecursive(RiftParserState& state, const BuilderExpr& expr);

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class ComparisonChain: public ExprElement {
			using Lower = GeneralBinary;

			std::vector<ParserRef<ExprElement>> sub_expr;
			std::vector<Operator>               operators;

			static u64 skipToOp(const RiftParserState& state, u64 base, u64 length);

		public:
			ComparisonChain(const dia::SourcePosition& pos): ExprElement(pos, 600){};

			std::string elementType() const override { return "Comparison Chain"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class LogicNot: public PrefixOperator {
			using Lower = ComparisonChain;
			using Self  = LogicNot;

		public:
			explicit LogicNot(const dia::SourcePosition& position):
				  PrefixOperator(position, rift_def::keywordToStr(Keyword::Not), 730) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class LogicAnd: public BinaryOperator {
			using Lower = LogicNot;
			using Self  = LogicAnd;

		public:
			explicit LogicAnd(const dia::SourcePosition& position):
				  BinaryOperator(position, rift_def::keywordToStr(rift_def::Keyword::And), 730) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class LogicOr: public BinaryOperator {
			using Lower = LogicAnd;
			using Self  = LogicOr;

		public:
			explicit LogicOr(const dia::SourcePosition& position):
				  BinaryOperator(position, rift_def::keywordToStr(rift_def::Keyword::Or), 760) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class Ternary: public ExprElement {
			using Lower = LogicOr;

			ParserRef<ExprElement> condition;
			ParserRef<ExprElement> if_true;
			ParserRef<ExprElement> if_false;

		public:
			explicit Ternary(const dia::SourcePosition& position): ExprElement(position, 800) {}

			std::string elementType() const override { return "Ternary Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class Comma: public ExprElement {
			using Lower = Ternary;

			std::vector<ParserRef<ExprElement>> expressions;

		public:
			explicit Comma(const dia::SourcePosition& position): ExprElement(position, 900) {}

			std::string elementType() const override { return "Comma Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};

		class Assignment: public ExprElement {
			using Lower = Comma;

			ParserRef<ExprElement> variables;
			base::StrId            type;
			ParserRef<ExprElement> value;

		public:
			explicit Assignment(const dia::SourcePosition& position):
				  ExprElement(position, 1'000) {}

			std::string elementType() const override { return "Assignment Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);
		};
	}

	class NewExprStmt: public Stmt {
		ParserRef<ExprElement> expr;

	public:
		explicit NewExprStmt(dia::SourcePosition pos): Stmt(StmtKind::ExprStmt, pos){};

		static ParserRef<NewExprStmt> parse(RiftParserState& state) {
			auto out = base::make_unique<NewExprStmt>(state.getPosition());
			state.parse(out)
				.with(&out->expr, expr::parseUntil<expr::Assignment, ExprClassify::exprStmtEnd>);
			return out;
		}

		std::string elementType() const override { return "New Expr Statement"; }

		void dprint(std::ostream& out) const override { out << "<NEWEXPRSTMT UNIMPLEMENTED>"; }

		void acceptVisitor(PstStmtVisitor&) const override { RIFT_PANIC("unimplemented"); }
	};
}
